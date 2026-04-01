#include "motion_ai.h"

#include <string.h>

typedef struct
{
  uint8_t initialized;
  uint8_t prob_history_count;
  uint8_t prob_history_next;
  uint16_t frames_since_infer;
  uint32_t abnormal_latch_remaining;
  uint32_t last_align_fail_total;
  float baseline_cache[MOTION_AI_BASELINE_MAX_FRAMES][MOTION_AI_FEATURE_COUNT];
  float baseline_sum[MOTION_AI_FEATURE_COUNT];
  float window[MOTION_AI_WINDOW_FRAMES][MOTION_AI_FEATURE_COUNT];
  float prob_history[MOTION_AI_SMOOTHING_WINDOW][MOTION_AI_CLASS_COUNT];
  float prob_avg[MOTION_AI_CLASS_COUNT];
  motion_ai_result_t result;
} motion_ai_context_t;

static motion_ai_context_t g_motion_ai;
static uint8_t g_motion_ai_single_test_enabled = MOTION_AI_SINGLE_TEST_MODE_DEFAULT;

static const char * const g_motion_ai_state_names[] = {
  "WAIT_START",
  "COLLECT_BASELINE",
  "FILL_WINDOW",
  "RUNNING",
  "TEST_DONE"
};

static const char * const g_motion_ai_label_names[] = {
  "rest",
  "elbow_flex",
  "front_raise",
  "side_raise",
  "shoulder_raise",
  "unknown"
};

static void motion_ai_reset_smoothing_history(void);
static void motion_ai_reset_context(void);
static void motion_ai_frame_to_raw(const motion_fused_frame_t *frame, float raw[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_update_align_fail(const motion_fused_frame_t *frame);
static void motion_ai_store_baseline_frame(const float raw[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_finalize_baseline(void);
static void motion_ai_compute_delta(
  const float raw[MOTION_AI_FEATURE_COUNT],
  float delta[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_update_motion_energy(const float delta[MOTION_AI_FEATURE_COUNT]);
static void motion_ai_append_window(const float delta[MOTION_AI_FEATURE_COUNT]);
static motion_label_t motion_ai_prob_index_to_label(uint32_t index);
static void motion_ai_update_prob_average(void);
static void motion_ai_update_latest_from_raw(const float probs[MOTION_AI_CLASS_COUNT]);
static void motion_ai_update_top1_from_average(void);
static void motion_ai_update_final_label(void);
static int motion_ai_run_inference(void);

void MotionAi_Init(void)
{
  motion_ai_reset_context();
  g_motion_ai.initialized = 1U;
}

void MotionAi_Reset(void)
{
  MotionAi_Init();
}

void MotionAi_SetSingleTestEnabled(uint8_t enabled)
{
  g_motion_ai_single_test_enabled = (enabled != 0U) ? 1U : 0U;
}

const motion_ai_result_t* MotionAi_ProcessFusedFrame(const motion_fused_frame_t *frame)
{
  float raw[MOTION_AI_FEATURE_COUNT];
  float delta[MOTION_AI_FEATURE_COUNT];

  if (!g_motion_ai.initialized)
  {
    MotionAi_Init();
  }

  if (frame == NULL)
  {
    return &g_motion_ai.result;
  }

  if (g_motion_ai.result.test_done != 0U)
  {
    return &g_motion_ai.result;
  }

  motion_ai_update_align_fail(frame);
  motion_ai_frame_to_raw(frame, raw);

  if (g_motion_ai.result.ai_state == MOTION_AI_STATE_WAIT_START)
  {
    g_motion_ai.result.ai_state = MOTION_AI_STATE_COLLECT_BASELINE;
  }

  if (g_motion_ai.result.ai_state == MOTION_AI_STATE_COLLECT_BASELINE)
  {
    motion_ai_store_baseline_frame(raw);
    motion_ai_update_final_label();
    return &g_motion_ai.result;
  }

  motion_ai_compute_delta(raw, delta);
  memcpy(g_motion_ai.result.delta, delta, sizeof(g_motion_ai.result.delta));
  motion_ai_update_motion_energy(delta);
  motion_ai_append_window(delta);

  if (g_motion_ai.result.ai_state == MOTION_AI_STATE_FILL_WINDOW)
  {
    if (g_motion_ai.result.window_count >= MOTION_AI_WINDOW_FRAMES)
    {
      (void)motion_ai_run_inference();
      g_motion_ai.result.ai_state = MOTION_AI_STATE_RUNNING;
      g_motion_ai.frames_since_infer = 0U;
    }
    else
    {
      motion_ai_update_final_label();
    }

    return &g_motion_ai.result;
  }

  if (g_motion_ai.frames_since_infer < 0xFFFFU)
  {
    g_motion_ai.frames_since_infer++;
  }

  if (g_motion_ai.frames_since_infer >= MOTION_AI_INFER_STRIDE)
  {
    (void)motion_ai_run_inference();
    g_motion_ai.frames_since_infer = 0U;
  }
  else
  {
    motion_ai_update_final_label();
  }

  return &g_motion_ai.result;
}

const motion_ai_result_t* MotionAi_GetResult(void)
{
  if (!g_motion_ai.initialized)
  {
    MotionAi_Init();
  }

  return &g_motion_ai.result;
}

const char* MotionAi_StateName(motion_ai_state_t state)
{
  if ((uint32_t)state < (sizeof(g_motion_ai_state_names) / sizeof(g_motion_ai_state_names[0])))
  {
    return g_motion_ai_state_names[(uint32_t)state];
  }

  return "UNKNOWN_STATE";
}

const char* MotionAi_LabelName(motion_label_t label)
{
  if ((uint32_t)label < (sizeof(g_motion_ai_label_names) / sizeof(g_motion_ai_label_names[0])))
  {
    return g_motion_ai_label_names[(uint32_t)label];
  }

  return "unknown";
}

static void motion_ai_reset_smoothing_history(void)
{
  memset(g_motion_ai.prob_history, 0, sizeof(g_motion_ai.prob_history));
  memset(g_motion_ai.prob_avg, 0, sizeof(g_motion_ai.prob_avg));
  memset(g_motion_ai.result.probs, 0, sizeof(g_motion_ai.result.probs));
  g_motion_ai.prob_history_count = 0U;
  g_motion_ai.prob_history_next = 0U;
  g_motion_ai.result.top1_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.top1_prob_avg = 0.0f;
  g_motion_ai.result.latest_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.latest_prob = 0.0f;
  g_motion_ai.result.smooth_ready = 0U;
}

static void motion_ai_reset_context(void)
{
  memset(&g_motion_ai, 0, sizeof(g_motion_ai));
  g_motion_ai.result.ai_state = MOTION_AI_STATE_WAIT_START;
  g_motion_ai.result.top1_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.final_label = MOTION_LABEL_UNKNOWN;
  g_motion_ai.result.latest_label = MOTION_LABEL_UNKNOWN;
  motion_ai_reset_smoothing_history();
}

static void motion_ai_frame_to_raw(const motion_fused_frame_t *frame, float raw[MOTION_AI_FEATURE_COUNT])
{
  raw[0] = frame->upper_yaw;
  raw[1] = frame->upper_pitch;
  raw[2] = frame->upper_roll;
  raw[3] = frame->fore_yaw;
  raw[4] = frame->fore_pitch;
  raw[5] = frame->fore_roll;
}

static void motion_ai_update_align_fail(const motion_fused_frame_t *frame)
{
  uint32_t align_fail_delta;

  g_motion_ai.result.align_fail_total = frame->align_fail_count;

  if (frame->align_fail_count >= g_motion_ai.last_align_fail_total)
  {
    align_fail_delta = frame->align_fail_count - g_motion_ai.last_align_fail_total;
  }
  else
  {
    align_fail_delta = frame->align_fail_count;
  }

  g_motion_ai.result.align_fail_delta = align_fail_delta;
  g_motion_ai.last_align_fail_total = frame->align_fail_count;

  if (align_fail_delta > 0U)
  {
    g_motion_ai.abnormal_latch_remaining = MOTION_AI_ABNORMAL_LATCH_FRAMES;
  }
  else if (g_motion_ai.abnormal_latch_remaining > 0U)
  {
    g_motion_ai.abnormal_latch_remaining--;
  }

  g_motion_ai.result.abnormal_flag = (g_motion_ai.abnormal_latch_remaining > 0U) ? 1U : 0U;
}

static void motion_ai_store_baseline_frame(const float raw[MOTION_AI_FEATURE_COUNT])
{
  uint16_t count = g_motion_ai.result.baseline_count;
  uint32_t idx;

  if (count < MOTION_AI_BASELINE_MAX_FRAMES)
  {
    memcpy(g_motion_ai.baseline_cache[count], raw, sizeof(g_motion_ai.baseline_cache[count]));
  }

  if (count < 0xFFFFU)
  {
    count++;
  }

  g_motion_ai.result.baseline_count = count;

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    g_motion_ai.baseline_sum[idx] += raw[idx];
  }

  memset(g_motion_ai.result.delta, 0, sizeof(g_motion_ai.result.delta));
  g_motion_ai.result.motion_energy = 0.0f;

  if (count >= MOTION_AI_BASELINE_TARGET_FRAMES)
  {
    motion_ai_finalize_baseline();
  }
}

static void motion_ai_finalize_baseline(void)
{
  uint32_t idx;
  float divisor = (float)MOTION_AI_BASELINE_TARGET_FRAMES;

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    g_motion_ai.result.base_mean[idx] = g_motion_ai.baseline_sum[idx] / divisor;
  }

  g_motion_ai.result.ai_state = MOTION_AI_STATE_FILL_WINDOW;
  g_motion_ai.result.window_count = 0U;
  g_motion_ai.frames_since_infer = 0U;
  motion_ai_reset_smoothing_history();
}

static void motion_ai_compute_delta(
  const float raw[MOTION_AI_FEATURE_COUNT],
  float delta[MOTION_AI_FEATURE_COUNT])
{
  uint32_t idx;

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    delta[idx] = raw[idx] - g_motion_ai.result.base_mean[idx];
  }
}

static void motion_ai_update_motion_energy(const float delta[MOTION_AI_FEATURE_COUNT])
{
  uint32_t idx;
  float energy = 0.0f;

  for (idx = 0U; idx < MOTION_AI_FEATURE_COUNT; idx++)
  {
    energy += delta[idx] * delta[idx];
  }

  g_motion_ai.result.motion_energy = energy / (float)MOTION_AI_FEATURE_COUNT;
}

static void motion_ai_append_window(const float delta[MOTION_AI_FEATURE_COUNT])
{
  if (g_motion_ai.result.window_count < MOTION_AI_WINDOW_FRAMES)
  {
    memcpy(g_motion_ai.window[g_motion_ai.result.window_count], delta, sizeof(g_motion_ai.window[0]));
    g_motion_ai.result.window_count++;
    return;
  }

  memmove(
    &g_motion_ai.window[0][0],
    &g_motion_ai.window[1][0],
    (MOTION_AI_WINDOW_FRAMES - 1U) * MOTION_AI_FEATURE_COUNT * sizeof(float));
  memcpy(g_motion_ai.window[MOTION_AI_WINDOW_FRAMES - 1U], delta, sizeof(g_motion_ai.window[0]));
}

static motion_label_t motion_ai_prob_index_to_label(uint32_t index)
{
  switch (index)
  {
    case 0U: return MOTION_LABEL_REST;
    case 1U: return MOTION_LABEL_ELBOW_FLEX;
    case 2U: return MOTION_LABEL_FRONT_RAISE;
    case 3U: return MOTION_LABEL_SIDE_RAISE;
    case 4U: return MOTION_LABEL_SHOULDER_RAISE;
    default: return MOTION_LABEL_UNKNOWN;
  }
}

static void motion_ai_update_prob_average(void)
{
  uint32_t row;
  uint32_t col;

  memset(g_motion_ai.prob_avg, 0, sizeof(g_motion_ai.prob_avg));

  if (g_motion_ai.prob_history_count == 0U)
  {
    return;
  }

  for (row = 0U; row < g_motion_ai.prob_history_count; row++)
  {
    for (col = 0U; col < MOTION_AI_CLASS_COUNT; col++)
    {
      g_motion_ai.prob_avg[col] += g_motion_ai.prob_history[row][col];
    }
  }

  for (col = 0U; col < MOTION_AI_CLASS_COUNT; col++)
  {
    g_motion_ai.prob_avg[col] /= (float)g_motion_ai.prob_history_count;
  }
}

static void motion_ai_update_latest_from_raw(const float probs[MOTION_AI_CLASS_COUNT])
{
  uint32_t idx;
  uint32_t best_index = 0U;
  float best_prob = probs[0];

  for (idx = 1U; idx < MOTION_AI_CLASS_COUNT; idx++)
  {
    if (probs[idx] > best_prob)
    {
      best_prob = probs[idx];
      best_index = idx;
    }
  }

  g_motion_ai.result.latest_label = motion_ai_prob_index_to_label(best_index);
  g_motion_ai.result.latest_prob = best_prob;
}

static void motion_ai_update_top1_from_average(void)
{
  uint32_t idx;
  uint32_t best_index = 0U;
  float best_prob = 0.0f;

  if (g_motion_ai.prob_history_count == 0U)
  {
    g_motion_ai.result.top1_label = MOTION_LABEL_UNKNOWN;
    g_motion_ai.result.top1_prob_avg = 0.0f;
    return;
  }

  best_prob = g_motion_ai.prob_avg[0];
  for (idx = 1U; idx < MOTION_AI_CLASS_COUNT; idx++)
  {
    if (g_motion_ai.prob_avg[idx] > best_prob)
    {
      best_prob = g_motion_ai.prob_avg[idx];
      best_index = idx;
    }
  }

  g_motion_ai.result.top1_label = motion_ai_prob_index_to_label(best_index);
  g_motion_ai.result.top1_prob_avg = best_prob;
}

static void motion_ai_update_final_label(void)
{
  if ((g_motion_ai_single_test_enabled != 0U) && (g_motion_ai.result.smooth_ready == 0U))
  {
    g_motion_ai.result.final_label = MOTION_LABEL_UNKNOWN;
    return;
  }

  if (g_motion_ai.result.motion_energy < MOTION_AI_E_REST_TH)
  {
    g_motion_ai.result.final_label = MOTION_LABEL_REST;
  }
  else if (g_motion_ai.result.top1_prob_avg >= MOTION_AI_P_KNOWN_TH)
  {
    g_motion_ai.result.final_label = g_motion_ai.result.top1_label;
  }
  else
  {
    g_motion_ai.result.final_label = MOTION_LABEL_UNKNOWN;
  }
}

static int motion_ai_run_inference(void)
{
  float output[MOTION_AI_CLASS_COUNT];

  if (AppXCubeAI_Run(&g_motion_ai.window[0][0], output) != 0)
  {
    motion_ai_reset_smoothing_history();
    motion_ai_update_final_label();
    return -1;
  }

  memcpy(g_motion_ai.result.probs, output, sizeof(g_motion_ai.result.probs));
  motion_ai_update_latest_from_raw(output);
  memcpy(g_motion_ai.prob_history[g_motion_ai.prob_history_next], output, sizeof(output));

  if (g_motion_ai.prob_history_count < MOTION_AI_SMOOTHING_WINDOW)
  {
    g_motion_ai.prob_history_count++;
  }

  g_motion_ai.prob_history_next++;
  if (g_motion_ai.prob_history_next >= MOTION_AI_SMOOTHING_WINDOW)
  {
    g_motion_ai.prob_history_next = 0U;
  }

  if (g_motion_ai.result.infer_count < 0xFFU)
  {
    g_motion_ai.result.infer_count++;
  }

  g_motion_ai.result.smooth_ready =
    (g_motion_ai.prob_history_count >= MOTION_AI_SMOOTHING_WINDOW) ? 1U : 0U;
  motion_ai_update_prob_average();
  motion_ai_update_top1_from_average();
  motion_ai_update_final_label();

  if ((g_motion_ai_single_test_enabled != 0U) &&
      (g_motion_ai.result.infer_count >= MOTION_AI_SMOOTHING_WINDOW) &&
      (g_motion_ai.result.smooth_ready != 0U))
  {
    g_motion_ai.result.test_done = 1U;
    g_motion_ai.result.ai_state = MOTION_AI_STATE_TEST_DONE;
  }

  return 0;
}
