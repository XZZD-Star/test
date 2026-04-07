#include "rule_action_recognizer.h"

#include <stddef.h>
#include <string.h>

#define RULE_INVALID_TEMPLATE_INDEX   (0xFFFFFFFFUL)
#define RULE_DISTANCE_INVALID         (1000000.0f)
#define RULE_ALT_AXIS_EXTRA_WEIGHT    (0.35f)
#define RULE_AMPLITUDE_FULL_SCORE     (35U)
#define RULE_PEAK_HOLD_FULL_SCORE     (25U)
#define RULE_COMPLETENESS_FULL_SCORE  (40U)

static const char * const g_rule_state_names[] = {
  "STATIC_WAIT",
  "READY",
  "ACTION_RISING",
  "PEAK_HOLD",
  "ACTION_FALLING",
  "ACTION_DONE"
};

static const char * const g_rule_action_names[] = {
  "unknown",
  "elbow_flex",
  "front_raise",
  "side_raise",
  "shoulder_raise"
};

static const char * const g_rule_grade_names[] = {
  "fail",
  "pass",
  "good",
  "excellent"
};

static const char * const g_rule_axis_names[] = {
  "upper_yaw",
  "upper_pitch",
  "upper_roll",
  "fore_yaw",
  "fore_pitch",
  "fore_roll"
};

static const ActionTemplate g_rule_default_templates[] = {
  {
    "elbow_flex",
    ACTION_ELBOW_FLEX,
    AXIS_FORE_PITCH,
    AXIS_UPPER_ROLL,
    AXIS_UPPER_YAW,
    { -140.4f,  10.7f,  65.7f, -38.1f, 107.8f,  70.4f },
    {  -67.3f,  31.0f, 139.9f, -13.6f, 141.9f,  96.4f },
    {   0U,      1U,     1U,     0U,     1U,     1U   },
    {   0.0f,  -73.3f,  69.2f,   0.0f,  35.0f, 144.7f },
    {   0.0f,  -51.8f,  82.1f,   0.0f,  63.4f, 155.7f },
    { -115.2f,  24.6f, 109.0f, -27.4f, 130.6f,  81.6f },
    {   99.6f, -59.8f,  74.7f, 115.5f,  52.8f, 149.4f },
    {    1.4f,   1.2f,   2.8f,   1.0f,   3.8f,   1.2f },
    {    0.4f,   2.0f,   3.2f,   0.4f,   4.5f,   1.8f },
    108.0f,
    124.0f,
    141.0f,
    RULE_DEFAULT_PEAK_HOLD_MIN_MS,
    RULE_DEFAULT_PEAK_HOLD_IDEAL_MIN_MS,
    RULE_DEFAULT_PEAK_HOLD_IDEAL_MAX_MS,
    RULE_DEFAULT_PEAK_HOLD_MAX_MS
  },
  {
    "front_raise",
    ACTION_FRONT_RAISE,
    AXIS_UPPER_YAW,
    AXIS_UPPER_ROLL,
    AXIS_FORE_PITCH,
    { -128.7f,  78.4f,  81.8f, -24.9f,  83.3f, 14.4f },
    {  -97.1f, 101.8f, 114.5f, -15.2f, 105.7f, 28.2f },
    {    1U,     1U,     1U,     1U,     0U,    1U   },
    { -100.4f,  -5.4f,  59.9f, -36.6f,   0.0f, 91.8f },
    {  -91.9f,  18.1f,  65.1f,  33.5f,   0.0f, 95.5f },
    { -114.7f,  83.4f,  98.6f, -17.2f,  88.0f, 18.2f },
    {  -97.0f,   0.0f,  63.0f,  -2.4f,   7.6f, 93.9f },
    {    3.8f,   1.0f,   2.8f,   0.8f,   1.3f,  0.8f },
    {    4.5f,   1.6f,   3.2f,   1.0f,   0.4f,  1.8f },
    97.0f,
    109.0f,
    124.0f,
    RULE_DEFAULT_PEAK_HOLD_MIN_MS,
    RULE_DEFAULT_PEAK_HOLD_IDEAL_MIN_MS,
    RULE_DEFAULT_PEAK_HOLD_IDEAL_MAX_MS,
    RULE_DEFAULT_PEAK_HOLD_MAX_MS
  },
  {
    "shoulder_raise",
    ACTION_SHOULDER_RAISE,
    AXIS_FORE_PITCH,
    AXIS_UPPER_PITCH,
    AXIS_UPPER_PITCH,
    { -79.6f, 133.7f, 55.0f,  -5.7f, 139.5f, -15.1f },
    { -56.5f, 144.8f, 78.1f,  13.1f, 150.9f,  32.6f },
    {   1U,     1U,    1U,     1U,     1U,     1U    },
    { -51.0f,  53.5f, 21.1f, -71.3f,  63.0f,  37.0f },
    {  12.3f,  62.9f, 28.3f,  19.0f,  71.6f,  52.6f },
    { -68.1f, 139.3f, 62.5f,   3.4f, 143.8f,   7.6f },
    { -23.2f,  57.8f, 26.0f, -34.4f,  69.3f,  42.4f },
    {   0.9f,   3.0f,  1.2f,   0.8f,   3.8f,   0.7f },
    {   1.0f,   3.8f,  1.4f,   0.8f,   4.5f,   1.0f },
    140.0f,
    142.0f,
    149.0f,
    RULE_DEFAULT_PEAK_HOLD_MIN_MS,
    RULE_DEFAULT_PEAK_HOLD_IDEAL_MIN_MS,
    RULE_DEFAULT_PEAK_HOLD_IDEAL_MAX_MS,
    RULE_DEFAULT_PEAK_HOLD_MAX_MS
  },
  {
    "side_raise",
    ACTION_SIDE_RAISE,
    AXIS_UPPER_PITCH,
    AXIS_FORE_PITCH,
    AXIS_FORE_PITCH,
    { -46.6f, 84.5f, 37.2f, 57.3f, 78.6f, -62.1f },
    { -17.3f, 89.1f, 69.3f, 73.4f, 89.6f, -36.8f },
    {   1U,    1U,    1U,    1U,    1U,    1U    },
    { -33.8f,  0.5f,  4.2f, -59.2f, 3.0f, 14.8f },
    {  53.9f,  5.7f, 10.7f,  63.1f, 8.4f, 23.3f },
    { -35.5f, 87.7f, 53.5f,  67.4f, 84.8f, -53.5f },
    {   1.5f,  4.5f,  7.5f,  -3.7f, 6.8f,  18.4f },
    {   0.8f,  3.6f,  1.2f,   1.0f, 3.0f,   1.0f },
    {   1.0f,  4.5f,  1.2f,   0.9f, 3.6f,   1.3f },
    84.5f,
    86.0f,
    89.0f,
    RULE_DEFAULT_PEAK_HOLD_MIN_MS,
    RULE_DEFAULT_PEAK_HOLD_IDEAL_MIN_MS,
    RULE_DEFAULT_PEAK_HOLD_IDEAL_MAX_MS,
    RULE_DEFAULT_PEAK_HOLD_MAX_MS
  }
};

static float rule_absf(float value);
static float rule_maxf(float a, float b);
static float rule_minf(float a, float b);
static uint32_t rule_elapsed_ms(uint32_t start_ms, uint32_t now_ms);
static float rule_normalize_linear(float value, float min_value, float max_value);
static uint16_t rule_scale_score(
  float value,
  float in_min,
  float in_max,
  uint16_t out_min,
  uint16_t out_max);
static uint8_t rule_value_in_range(float value, float min_value, float max_value);
static void rule_reset_result(ActionResult *result);
static void rule_copy_result_features(ActionResult *result, const ActionSession *session);
static void rule_reset_static_accumulator(RuleEngine *eng);
static void rule_compute_delta(RuleEngine *eng);
static void rule_lock_baseline(RuleEngine *eng);
static void rule_update_static_candidate(RuleEngine *eng, const float raw[AXIS_COUNT]);
static void rule_store_session_frame(
  ActionSession *session,
  const float raw[AXIS_COUNT],
  const float delta[AXIS_COUNT]);
static void rule_begin_action(RuleEngine *eng);
static void rule_finalize_action(RuleEngine *eng, uint8_t returned_to_static);
static uint8_t rule_session_timed_out(const RuleEngine *eng);
static AxisIndex rule_find_dominant_axis(const ActionSession *session);

void RuleConfig_LoadDefault(RuleConfig *cfg)
{
  if (cfg == NULL)
  {
    return;
  }

  cfg->static_energy_th = 6.0f;
  cfg->n_static_frames = 10U;
  cfg->start_energy_th = RULE_DEFAULT_START_ENERGY_TH;
  cfg->start_confirm_frames = RULE_DEFAULT_START_CONFIRM_FRAMES;

  cfg->peak_enter_amp_th = 18.0f;
  cfg->peak_stable_delta_th = 1.5f;
  cfg->peak_stable_frames = 3U;
  cfg->peak_exit_drop_th = 2.0f;
  cfg->peak_exit_min_hold_ms = RULE_DEFAULT_PEAK_EXIT_MIN_HOLD_MS;
  cfg->peak_exit_confirm_frames = RULE_DEFAULT_PEAK_EXIT_CONFIRM_FRAMES;

  cfg->return_energy_th = 8.0f;
  cfg->return_axis_th = 5.0f;
  cfg->return_stable_frames = 4U;

  cfg->action_timeout_ms = 4500U;
}

void RuleEngine_Init(RuleEngine *eng, const RuleConfig *cfg)
{
  RuleConfig default_cfg;

  if (eng == NULL)
  {
    return;
  }

  memset(eng, 0, sizeof(*eng));
  RuleConfig_LoadDefault(&default_cfg);
  eng->cfg = (cfg != NULL) ? (*cfg) : default_cfg;
  eng->state = RULE_STATE_STATIC_WAIT;
  RuleEngine_SetTemplates(eng, NULL, 0U);
  RuleEngine_ResetSession(eng);
  eng->initialized = 1U;
}

void RuleEngine_ResetSession(RuleEngine *eng)
{
  if (eng == NULL)
  {
    return;
  }

  memset(&eng->session, 0, sizeof(eng->session));
  eng->session.dominant_axis = AXIS_UPPER_YAW;
  eng->start_confirm_count = 0U;
  eng->peak_stable_count = 0U;
  eng->peak_exit_confirm_count = 0U;
  eng->return_stable_count = 0U;
  rule_reset_result(&eng->result);
}

void RuleEngine_SetTemplates(
  RuleEngine *eng,
  const ActionTemplate *templates,
  uint32_t template_count)
{
  if (eng == NULL)
  {
    return;
  }

  if ((templates == NULL) || (template_count == 0U))
  {
    eng->templates = g_rule_default_templates;
    eng->template_count =
      (uint32_t)(sizeof(g_rule_default_templates) / sizeof(g_rule_default_templates[0]));
    return;
  }

  eng->templates = templates;
  eng->template_count = template_count;
}

void RuleEngine_Update(
  RuleEngine *eng,
  const float raw[AXIS_COUNT],
  float motion_energy,
  uint32_t now_ms)
{
  if ((eng == NULL) || (raw == NULL))
  {
    return;
  }

  if (eng->initialized == 0U)
  {
    RuleEngine_Init(eng, NULL);
  }

  memcpy(eng->raw, raw, sizeof(eng->raw));
  eng->motion_energy = motion_energy;
  eng->now_ms = now_ms;

  if (eng->baseline_valid != 0U)
  {
    rule_compute_delta(eng);
  }
  else
  {
    memset(eng->delta, 0, sizeof(eng->delta));
  }

  if (eng->state == RULE_STATE_ACTION_DONE)
  {
    eng->state = RULE_STATE_STATIC_WAIT;
    eng->baseline_valid = 0U;
    rule_reset_static_accumulator(eng);
    RuleEngine_ResetSession(eng);
    memset(eng->delta, 0, sizeof(eng->delta));
  }

  switch (eng->state)
  {
    case RULE_STATE_STATIC_WAIT:
      if (motion_energy < eng->cfg.static_energy_th)
      {
        rule_update_static_candidate(eng, raw);
        if (eng->static_frame_count >= eng->cfg.n_static_frames)
        {
          rule_lock_baseline(eng);
          eng->state = RULE_STATE_READY;
        }
      }
      else
      {
        rule_reset_static_accumulator(eng);
      }
      break;

    case RULE_STATE_READY:
      if (motion_energy > eng->cfg.start_energy_th)
      {
        if (eng->start_confirm_count < 0xFFFFU)
        {
          eng->start_confirm_count++;
        }

        if (eng->start_confirm_count >= eng->cfg.start_confirm_frames)
        {
          eng->start_confirm_count = 0U;
          rule_begin_action(eng);
        }
      }
      else
      {
        eng->start_confirm_count = 0U;
      }
      break;

    case RULE_STATE_ACTION_RISING:
      Rule_UpdateAmplitudeStats(&eng->session, eng->delta);
      eng->session.rise_time_ms = rule_elapsed_ms(
        eng->session.rise_start_ms,
        eng->now_ms);
      eng->session.total_time_ms = rule_elapsed_ms(
        eng->session.start_ms,
        eng->now_ms);
      rule_store_session_frame(&eng->session, eng->raw, eng->delta);

      if (Rule_ShouldEnterPeakHold(eng) != 0U)
      {
        eng->session.has_peak_hold = 1U;
        eng->session.peak_enter_ms = eng->now_ms;
        eng->session.peak_hold_ms = 0U;
        eng->peak_exit_confirm_count = 0U;
        Rule_UpdatePeakStats(&eng->session, eng->delta);
        eng->state = RULE_STATE_PEAK_HOLD;
      }
      else if ((motion_energy < eng->cfg.return_energy_th) &&
               (eng->session.dominant_amp >= (eng->cfg.peak_enter_amp_th * 0.5f)))
      {
        eng->session.has_falling_phase = 1U;
        eng->session.fall_start_ms = eng->now_ms;
        eng->session.fall_time_ms = 0U;
        eng->state = RULE_STATE_ACTION_FALLING;
      }
      break;

    case RULE_STATE_PEAK_HOLD:
      Rule_UpdateAmplitudeStats(&eng->session, eng->delta);
      Rule_UpdatePeakStats(&eng->session, eng->delta);
      eng->session.peak_hold_ms = rule_elapsed_ms(
        eng->session.peak_enter_ms,
        eng->now_ms);
      eng->session.total_time_ms = rule_elapsed_ms(
        eng->session.start_ms,
        eng->now_ms);
      rule_store_session_frame(&eng->session, eng->raw, eng->delta);

      if (Rule_ShouldExitPeakHold(eng) != 0U)
      {
        eng->session.has_falling_phase = 1U;
        eng->session.fall_start_ms = eng->now_ms;
        eng->session.fall_time_ms = 0U;
        eng->state = RULE_STATE_ACTION_FALLING;
      }
      break;

    case RULE_STATE_ACTION_FALLING:
      Rule_UpdateAmplitudeStats(&eng->session, eng->delta);
      eng->session.fall_time_ms = rule_elapsed_ms(
        eng->session.fall_start_ms,
        eng->now_ms);
      eng->session.total_time_ms = rule_elapsed_ms(
        eng->session.start_ms,
        eng->now_ms);
      rule_store_session_frame(&eng->session, eng->raw, eng->delta);

      if (Rule_IsActionReturned(eng) != 0U)
      {
        rule_finalize_action(eng, 1U);
        eng->state = RULE_STATE_ACTION_DONE;
      }
      break;

    case RULE_STATE_ACTION_DONE:
    default:
      break;
  }

  if ((eng->state == RULE_STATE_ACTION_RISING) ||
      (eng->state == RULE_STATE_PEAK_HOLD) ||
      (eng->state == RULE_STATE_ACTION_FALLING))
  {
    if (rule_session_timed_out(eng) != 0U)
    {
      eng->result.timed_out = 1U;
      rule_finalize_action(eng, 0U);
      eng->state = RULE_STATE_ACTION_DONE;
    }
  }

  memcpy(eng->prev_delta, eng->delta, sizeof(eng->prev_delta));
  eng->has_prev_raw = 1U;
}

void RuleEngine_ProcessRaw(
  RuleEngine *eng,
  const float raw[AXIS_COUNT],
  uint32_t now_ms)
{
  float motion_energy;

  if ((eng == NULL) || (raw == NULL))
  {
    return;
  }

  motion_energy = Rule_ComputeMotionEnergy(eng, raw);
  RuleEngine_Update(eng, raw, motion_energy, now_ms);
}

float Rule_ComputeMotionEnergy(const RuleEngine *eng, const float raw[AXIS_COUNT])
{
  const float *reference = NULL;
  float energy = 0.0f;
  uint32_t axis;

  if ((eng == NULL) || (raw == NULL))
  {
    return 0.0f;
  }

  if (eng->baseline_valid != 0U)
  {
    reference = eng->base_mean;
  }
  else if (eng->has_prev_raw != 0U)
  {
    reference = eng->raw;
  }
  else
  {
    return 0.0f;
  }

  for (axis = 0U; axis < AXIS_COUNT; axis++)
  {
    float diff = raw[axis] - reference[axis];
    energy += diff * diff;
  }

  return energy / (float)AXIS_COUNT;
}

void Rule_UpdateAmplitudeStats(ActionSession *session, const float delta[AXIS_COUNT])
{
  uint32_t axis;

  if ((session == NULL) || (delta == NULL))
  {
    return;
  }

  for (axis = 0U; axis < AXIS_COUNT; axis++)
  {
    if (session->frame_count == 0U)
    {
      session->delta_max[axis] = delta[axis];
      session->delta_min[axis] = delta[axis];
    }
    else
    {
      if (delta[axis] > session->delta_max[axis])
      {
        session->delta_max[axis] = delta[axis];
      }

      if (delta[axis] < session->delta_min[axis])
      {
        session->delta_min[axis] = delta[axis];
      }
    }

    if (rule_absf(session->delta_max[axis]) >= rule_absf(session->delta_min[axis]))
    {
      session->amp[axis] = session->delta_max[axis];
    }
    else
    {
      session->amp[axis] = session->delta_min[axis];
    }

    session->amp_abs[axis] = rule_absf(session->amp[axis]);
  }

  session->frame_count++;
  session->dominant_axis = rule_find_dominant_axis(session);
  session->dominant_amp = session->amp_abs[(uint32_t)session->dominant_axis];
}

void Rule_UpdatePeakStats(ActionSession *session, const float delta[AXIS_COUNT])
{
  uint32_t axis;
  float divisor;

  if ((session == NULL) || (delta == NULL))
  {
    return;
  }

  session->peak_count++;
  divisor = (float)session->peak_count;

  for (axis = 0U; axis < AXIS_COUNT; axis++)
  {
    session->peak_sum[axis] += delta[axis];
    session->peak_mean[axis] = session->peak_sum[axis] / divisor;
  }
}

uint8_t Rule_ShouldEnterPeakHold(RuleEngine *eng)
{
  AxisIndex main_axis;
  float current_abs;
  float prev_abs;
  float diff_abs;

  if ((eng == NULL) || (eng->session.active == 0U))
  {
    return 0U;
  }

  if (eng->session.frame_count < 2U)
  {
    eng->peak_stable_count = 0U;
    return 0U;
  }

  main_axis = eng->session.dominant_axis;
  current_abs = rule_absf(eng->delta[(uint32_t)main_axis]);
  prev_abs = rule_absf(eng->prev_delta[(uint32_t)main_axis]);
  diff_abs = rule_absf(current_abs - prev_abs);

  if ((eng->session.dominant_amp >= eng->cfg.peak_enter_amp_th) &&
      (diff_abs <= eng->cfg.peak_stable_delta_th))
  {
    if (eng->peak_stable_count < 0xFFFFU)
    {
      eng->peak_stable_count++;
    }
  }
  else
  {
    eng->peak_stable_count = 0U;
  }

  if (eng->peak_stable_count >= eng->cfg.peak_stable_frames)
  {
    eng->peak_stable_count = 0U;
    return 1U;
  }

  return 0U;
}

uint8_t Rule_ShouldExitPeakHold(RuleEngine *eng)
{
  AxisIndex main_axis;
  float current_abs;
  float prev_abs;

  if ((eng == NULL) || (eng->session.active == 0U))
  {
    return 0U;
  }

  main_axis = eng->session.dominant_axis;
  current_abs = rule_absf(eng->delta[(uint32_t)main_axis]);
  prev_abs = rule_absf(eng->prev_delta[(uint32_t)main_axis]);

  if (eng->session.peak_hold_ms < eng->cfg.peak_exit_min_hold_ms)
  {
    eng->peak_exit_confirm_count = 0U;
    return 0U;
  }

  if ((prev_abs > current_abs) &&
      ((prev_abs - current_abs) >= eng->cfg.peak_exit_drop_th))
  {
    if (eng->peak_exit_confirm_count < 0xFFFFU)
    {
      eng->peak_exit_confirm_count++;
    }

    if (eng->peak_exit_confirm_count >= eng->cfg.peak_exit_confirm_frames)
    {
      eng->peak_exit_confirm_count = 0U;
      return 1U;
    }
  }
  else
  {
    eng->peak_exit_confirm_count = 0U;
  }

  return 0U;
}

uint8_t Rule_IsActionReturned(RuleEngine *eng)
{
  AxisIndex main_axis;
  float axis_abs;

  if ((eng == NULL) || (eng->session.active == 0U))
  {
    return 0U;
  }

  main_axis = eng->session.dominant_axis;
  axis_abs = rule_absf(eng->delta[(uint32_t)main_axis]);

  if ((eng->motion_energy <= eng->cfg.return_energy_th) &&
      (axis_abs <= eng->cfg.return_axis_th))
  {
    if (eng->return_stable_count < 0xFFFFU)
    {
      eng->return_stable_count++;
    }
  }
  else
  {
    eng->return_stable_count = 0U;
  }

  if (eng->return_stable_count >= eng->cfg.return_stable_frames)
  {
    eng->return_stable_count = 0U;
    return 1U;
  }

  return 0U;
}

uint8_t Rule_FilterTemplate(
  const ActionSession *session,
  const ActionTemplate *tmpl)
{
  uint32_t axis;

  if ((session == NULL) || (tmpl == NULL))
  {
    return 0U;
  }

  if (session->frame_count == 0U)
  {
    return 0U;
  }

  if (rule_value_in_range(
        session->amp[(uint32_t)tmpl->main_axis],
        tmpl->amp_min[(uint32_t)tmpl->main_axis],
        tmpl->amp_max[(uint32_t)tmpl->main_axis]) == 0U)
  {
    return 0U;
  }

  if (rule_value_in_range(
        session->amp[(uint32_t)tmpl->sub_axis],
        tmpl->amp_min[(uint32_t)tmpl->sub_axis],
        tmpl->amp_max[(uint32_t)tmpl->sub_axis]) == 0U)
  {
    return 0U;
  }

  for (axis = 0U; axis < AXIS_COUNT; axis++)
  {
    if (tmpl->peak_range_enabled[axis] == 0U)
    {
      continue;
    }

    if (session->peak_count == 0U)
    {
      return 0U;
    }

    if (rule_value_in_range(
          session->peak_mean[axis],
          tmpl->peak_mean_min[axis],
          tmpl->peak_mean_max[axis]) == 0U)
    {
      return 0U;
    }
  }

  return 1U;
}

float Rule_ComputeTemplateMatchScore(
  const ActionSession *session,
  const ActionTemplate *tmpl,
  float *amp_distance,
  float *peak_distance)
{
  float dist_amp = 0.0f;
  float dist_peak = 0.0f;
  uint32_t axis;

  if ((session == NULL) || (tmpl == NULL))
  {
    if (amp_distance != NULL)
    {
      *amp_distance = RULE_DISTANCE_INVALID;
    }
    if (peak_distance != NULL)
    {
      *peak_distance = RULE_DISTANCE_INVALID;
    }
    return RULE_DISTANCE_INVALID;
  }

  for (axis = 0U; axis < AXIS_COUNT; axis++)
  {
    dist_amp += tmpl->amp_weight[axis] *
      rule_absf(session->amp[axis] - tmpl->amp_ref[axis]);

    if (session->peak_count != 0U)
    {
      dist_peak += tmpl->peak_weight[axis] *
        rule_absf(session->peak_mean[axis] - tmpl->peak_mean_ref[axis]);
    }
  }

  dist_amp += RULE_ALT_AXIS_EXTRA_WEIGHT *
    rule_absf(session->amp[(uint32_t)tmpl->alt_main_axis] -
              tmpl->amp_ref[(uint32_t)tmpl->alt_main_axis]);

  if (session->peak_count != 0U)
  {
    dist_peak += (RULE_ALT_AXIS_EXTRA_WEIGHT * 0.5f) *
      rule_absf(session->peak_mean[(uint32_t)tmpl->alt_main_axis] -
                tmpl->peak_mean_ref[(uint32_t)tmpl->alt_main_axis]);
  }

  if (amp_distance != NULL)
  {
    *amp_distance = dist_amp;
  }

  if (peak_distance != NULL)
  {
    *peak_distance = dist_peak;
  }

  return dist_amp + dist_peak;
}

ActionType Rule_RecognizeAction(
  const ActionSession *session,
  const ActionTemplate *templates,
  uint32_t template_count,
  ActionResult *out_result)
{
  uint32_t idx;
  uint32_t debug_count;
  float best_distance = RULE_DISTANCE_INVALID;
  uint32_t best_index = RULE_INVALID_TEMPLATE_INDEX;

  if ((session == NULL) || (templates == NULL) || (template_count == 0U))
  {
    if (out_result != NULL)
    {
      rule_reset_result(out_result);
      rule_copy_result_features(out_result, session);
    }
    return ACTION_UNKNOWN;
  }

  if (out_result != NULL)
  {
    rule_copy_result_features(out_result, session);
    out_result->action = ACTION_UNKNOWN;
    out_result->matched_template_name = NULL;
    out_result->matched_template = NULL;
    out_result->matched_template_index = RULE_INVALID_TEMPLATE_INDEX;
    out_result->match_score = RULE_DISTANCE_INVALID;
    out_result->primary_axis = session->dominant_axis;
    out_result->secondary_axis = session->dominant_axis;
    out_result->alt_primary_axis = session->dominant_axis;
    out_result->primary_axis_amp =
      session->amp_abs[(uint32_t)session->dominant_axis];
    out_result->template_debug_count = 0U;
  }

  debug_count = (uint32_t)rule_minf(
    (float)template_count,
    (float)RULE_TEMPLATE_DEBUG_MAX_COUNT);

  for (idx = 0U; idx < template_count; idx++)
  {
    uint8_t passed;
    float amp_distance = RULE_DISTANCE_INVALID;
    float peak_distance = RULE_DISTANCE_INVALID;
    float total_distance = RULE_DISTANCE_INVALID;

    passed = Rule_FilterTemplate(session, &templates[idx]);
    if (passed != 0U)
    {
      total_distance = Rule_ComputeTemplateMatchScore(
        session,
        &templates[idx],
        &amp_distance,
        &peak_distance);

      if (total_distance < best_distance)
      {
        best_distance = total_distance;
        best_index = idx;
      }
    }

    if ((out_result != NULL) && (idx < debug_count))
    {
      out_result->template_debug[idx].name = templates[idx].name;
      out_result->template_debug[idx].action_type = templates[idx].action_type;
      out_result->template_debug[idx].filter_passed = passed;
      out_result->template_debug[idx].distance_score = total_distance;
      out_result->template_debug[idx].amp_distance = amp_distance;
      out_result->template_debug[idx].peak_distance = peak_distance;
    }
  }

  if (out_result != NULL)
  {
    out_result->template_debug_count = debug_count;
  }

  if (best_index == RULE_INVALID_TEMPLATE_INDEX)
  {
    return ACTION_UNKNOWN;
  }

  if (out_result != NULL)
  {
    const ActionTemplate *best_template = &templates[best_index];

    out_result->action = best_template->action_type;
    out_result->matched_template_name = best_template->name;
    out_result->matched_template = best_template;
    out_result->matched_template_index = best_index;
    out_result->match_score = best_distance;
    out_result->primary_axis = best_template->main_axis;
    out_result->secondary_axis = best_template->sub_axis;
    out_result->alt_primary_axis = best_template->alt_main_axis;
    out_result->primary_axis_amp =
      session->amp_abs[(uint32_t)best_template->main_axis];
  }

  return templates[best_index].action_type;
}

uint16_t Rule_ScoreCompleteness(const ActionSession *session)
{
  if (session == NULL)
  {
    return 0U;
  }

  if ((session->has_static_start != 0U) &&
      (session->has_rising_phase != 0U) &&
      (session->has_peak_hold != 0U) &&
      (session->has_falling_phase != 0U) &&
      (session->returned_to_static != 0U))
  {
    return RULE_COMPLETENESS_FULL_SCORE;
  }

  return 0U;
}

uint16_t Rule_ScoreAmplitude(
  const ActionSession *session,
  const ActionTemplate *tmpl)
{
  float axis_amp;
  float overshoot_margin;

  if ((session == NULL) || (tmpl == NULL))
  {
    return 0U;
  }

  axis_amp = session->amp_abs[(uint32_t)tmpl->main_axis];
  overshoot_margin = rule_maxf(12.0f, tmpl->amp_ideal_max * 0.20f);

  if (axis_amp < tmpl->amp_min_required)
  {
    return rule_scale_score(
      axis_amp,
      0.0f,
      tmpl->amp_min_required,
      0U,
      12U);
  }

  if (axis_amp < tmpl->amp_ideal_min)
  {
    return rule_scale_score(
      axis_amp,
      tmpl->amp_min_required,
      tmpl->amp_ideal_min,
      15U,
      30U);
  }

  if (axis_amp <= tmpl->amp_ideal_max)
  {
    return RULE_AMPLITUDE_FULL_SCORE;
  }

  if (axis_amp <= (tmpl->amp_ideal_max + overshoot_margin))
  {
    return rule_scale_score(
      axis_amp,
      tmpl->amp_ideal_max,
      tmpl->amp_ideal_max + overshoot_margin,
      RULE_AMPLITUDE_FULL_SCORE,
      24U);
  }

  return 18U;
}

uint16_t Rule_ScorePeakHold(
  const ActionSession *session,
  const ActionTemplate *tmpl)
{
  float hold_ms;

  if ((session == NULL) || (tmpl == NULL))
  {
    return 0U;
  }

  if (session->peak_count == 0U)
  {
    return 0U;
  }

  hold_ms = (float)session->peak_hold_ms;

  if (hold_ms < (float)tmpl->peak_hold_min_ms)
  {
    return rule_scale_score(
      hold_ms,
      0.0f,
      (float)tmpl->peak_hold_min_ms,
      0U,
      8U);
  }

  if (hold_ms < (float)tmpl->peak_hold_ideal_min_ms)
  {
    return rule_scale_score(
      hold_ms,
      (float)tmpl->peak_hold_min_ms,
      (float)tmpl->peak_hold_ideal_min_ms,
      10U,
      22U);
  }

  if (hold_ms <= (float)tmpl->peak_hold_ideal_max_ms)
  {
    return RULE_PEAK_HOLD_FULL_SCORE;
  }

  if (hold_ms <= (float)tmpl->peak_hold_max_ms)
  {
    return rule_scale_score(
      hold_ms,
      (float)tmpl->peak_hold_ideal_max_ms,
      (float)tmpl->peak_hold_max_ms,
      22U,
      12U);
  }

  return 6U;
}

void Rule_EvaluateResult(
  const RuleConfig *cfg,
  const ActionSession *session,
  ActionResult *out_result)
{
  const ActionTemplate *tmpl;
  uint16_t total_score;

  (void)cfg;

  if ((session == NULL) || (out_result == NULL))
  {
    return;
  }

  tmpl = out_result->matched_template;

  out_result->completeness_score = Rule_ScoreCompleteness(session);
  out_result->amplitude_score = Rule_ScoreAmplitude(session, tmpl);
  out_result->peak_hold_score = Rule_ScorePeakHold(session, tmpl);
  out_result->complete =
    (out_result->completeness_score >= RULE_COMPLETENESS_FULL_SCORE) ? 1U : 0U;

  total_score = (uint16_t)(
    out_result->completeness_score +
    out_result->amplitude_score +
    out_result->peak_hold_score);
  out_result->score = total_score;

  if (total_score >= 85U)
  {
    out_result->grade = RULE_GRADE_EXCELLENT;
  }
  else if (total_score >= 70U)
  {
    out_result->grade = RULE_GRADE_GOOD;
  }
  else if (total_score >= 60U)
  {
    out_result->grade = RULE_GRADE_PASS;
  }
  else
  {
    out_result->grade = RULE_GRADE_FAIL;
  }
}

const ActionTemplate* Rule_GetDefaultTemplates(uint32_t *count)
{
  if (count != NULL)
  {
    *count = (uint32_t)(sizeof(g_rule_default_templates) / sizeof(g_rule_default_templates[0]));
  }

  return g_rule_default_templates;
}

const ActionResult* RuleEngine_GetResult(const RuleEngine *eng)
{
  if (eng == NULL)
  {
    return NULL;
  }

  return &eng->result;
}

const ActionSession* RuleEngine_GetSession(const RuleEngine *eng)
{
  if (eng == NULL)
  {
    return NULL;
  }

  return &eng->session;
}

const char* Rule_StateName(RuleState state)
{
  if ((uint32_t)state < (sizeof(g_rule_state_names) / sizeof(g_rule_state_names[0])))
  {
    return g_rule_state_names[(uint32_t)state];
  }

  return "UNKNOWN_STATE";
}

const char* Rule_ActionName(ActionType action)
{
  if ((uint32_t)action < (sizeof(g_rule_action_names) / sizeof(g_rule_action_names[0])))
  {
    return g_rule_action_names[(uint32_t)action];
  }

  return "unknown";
}

const char* Rule_GradeName(RuleGrade grade)
{
  if ((uint32_t)grade < (sizeof(g_rule_grade_names) / sizeof(g_rule_grade_names[0])))
  {
    return g_rule_grade_names[(uint32_t)grade];
  }

  return "fail";
}

const char* Rule_AxisName(AxisIndex axis)
{
  if ((uint32_t)axis < (sizeof(g_rule_axis_names) / sizeof(g_rule_axis_names[0])))
  {
    return g_rule_axis_names[(uint32_t)axis];
  }

  return "unknown_axis";
}

static float rule_absf(float value)
{
  return (value >= 0.0f) ? value : -value;
}

static float rule_maxf(float a, float b)
{
  return (a > b) ? a : b;
}

static float rule_minf(float a, float b)
{
  return (a < b) ? a : b;
}

static uint32_t rule_elapsed_ms(uint32_t start_ms, uint32_t now_ms)
{
  return now_ms - start_ms;
}

static float rule_normalize_linear(float value, float min_value, float max_value)
{
  if (max_value <= min_value)
  {
    return 1.0f;
  }

  if (value <= min_value)
  {
    return 0.0f;
  }

  if (value >= max_value)
  {
    return 1.0f;
  }

  return (value - min_value) / (max_value - min_value);
}

static uint16_t rule_scale_score(
  float value,
  float in_min,
  float in_max,
  uint16_t out_min,
  uint16_t out_max)
{
  float ratio;
  float score;

  ratio = rule_normalize_linear(value, in_min, in_max);
  score = (float)out_min + ratio * (float)((int32_t)out_max - (int32_t)out_min);

  if (score < 0.0f)
  {
    score = 0.0f;
  }

  if (score > 100.0f)
  {
    score = 100.0f;
  }

  return (uint16_t)(score + 0.5f);
}

static uint8_t rule_value_in_range(float value, float min_value, float max_value)
{
  float low;
  float high;

  low = rule_minf(min_value, max_value);
  high = rule_maxf(min_value, max_value);

  if ((value < low) || (value > high))
  {
    return 0U;
  }

  return 1U;
}

static void rule_reset_result(ActionResult *result)
{
  if (result == NULL)
  {
    return;
  }

  memset(result, 0, sizeof(*result));
  result->action = ACTION_UNKNOWN;
  result->grade = RULE_GRADE_FAIL;
  result->matched_template_index = RULE_INVALID_TEMPLATE_INDEX;
  result->match_score = RULE_DISTANCE_INVALID;
  result->primary_axis = AXIS_UPPER_YAW;
  result->secondary_axis = AXIS_UPPER_YAW;
  result->alt_primary_axis = AXIS_UPPER_YAW;
}

static void rule_copy_result_features(ActionResult *result, const ActionSession *session)
{
  if ((result == NULL) || (session == NULL))
  {
    return;
  }

  memcpy(result->amp, session->amp, sizeof(result->amp));
  memcpy(result->peak_mean, session->peak_mean, sizeof(result->peak_mean));
  result->peak_hold_ms = session->peak_hold_ms;
  result->total_time_ms = session->total_time_ms;
}

static void rule_reset_static_accumulator(RuleEngine *eng)
{
  if (eng == NULL)
  {
    return;
  }

  memset(eng->static_sum, 0, sizeof(eng->static_sum));
  eng->static_frame_count = 0U;
}

static void rule_compute_delta(RuleEngine *eng)
{
  uint32_t axis;

  for (axis = 0U; axis < AXIS_COUNT; axis++)
  {
    eng->delta[axis] = eng->raw[axis] - eng->base_mean[axis];
  }
}

static void rule_lock_baseline(RuleEngine *eng)
{
  uint32_t axis;
  float divisor;

  if ((eng == NULL) || (eng->static_frame_count == 0U))
  {
    return;
  }

  divisor = (float)eng->static_frame_count;
  for (axis = 0U; axis < AXIS_COUNT; axis++)
  {
    eng->base_mean[axis] = eng->static_sum[axis] / divisor;
  }

  eng->baseline_valid = 1U;
  rule_compute_delta(eng);
}

static void rule_update_static_candidate(RuleEngine *eng, const float raw[AXIS_COUNT])
{
  uint32_t axis;

  if ((eng == NULL) || (raw == NULL))
  {
    return;
  }

  if (eng->static_frame_count == 0U)
  {
    memset(eng->static_sum, 0, sizeof(eng->static_sum));
  }

  for (axis = 0U; axis < AXIS_COUNT; axis++)
  {
    eng->static_sum[axis] += raw[axis];
  }

  if (eng->static_frame_count < 0xFFFFU)
  {
    eng->static_frame_count++;
  }
}

static void rule_store_session_frame(
  ActionSession *session,
  const float raw[AXIS_COUNT],
  const float delta[AXIS_COUNT])
{
  if ((session == NULL) || (raw == NULL) || (delta == NULL))
  {
    return;
  }

  memcpy(session->last_raw, raw, sizeof(session->last_raw));
  memcpy(session->last_delta, delta, sizeof(session->last_delta));
}

static void rule_begin_action(RuleEngine *eng)
{
  if (eng == NULL)
  {
    return;
  }

  RuleEngine_ResetSession(eng);

  eng->session.active = 1U;
  eng->session.has_static_start = (eng->baseline_valid != 0U) ? 1U : 0U;
  eng->session.has_rising_phase = 1U;
  eng->session.start_ms = eng->now_ms;
  eng->session.rise_start_ms = eng->now_ms;
  memcpy(eng->session.base_mean, eng->base_mean, sizeof(eng->session.base_mean));

  Rule_UpdateAmplitudeStats(&eng->session, eng->delta);
  rule_store_session_frame(&eng->session, eng->raw, eng->delta);
  eng->state = RULE_STATE_ACTION_RISING;
}

static void rule_finalize_action(RuleEngine *eng, uint8_t returned_to_static)
{
  ActionType action_type;

  if (eng == NULL)
  {
    return;
  }

  eng->session.active = 0U;
  eng->session.returned_to_static = returned_to_static;
  eng->session.end_ms = eng->now_ms;
  eng->session.total_time_ms = rule_elapsed_ms(
    eng->session.start_ms,
    eng->session.end_ms);

  if ((eng->session.has_peak_hold != 0U) &&
      (eng->session.fall_start_ms >= eng->session.peak_enter_ms))
  {
    eng->session.peak_hold_ms = rule_elapsed_ms(
      eng->session.peak_enter_ms,
      eng->session.fall_start_ms);
  }

  if ((eng->session.has_falling_phase != 0U) &&
      (eng->session.end_ms >= eng->session.fall_start_ms))
  {
    eng->session.fall_time_ms = rule_elapsed_ms(
      eng->session.fall_start_ms,
      eng->session.end_ms);
  }

  eng->result.valid = 1U;
  rule_copy_result_features(&eng->result, &eng->session);

  action_type = Rule_RecognizeAction(
    &eng->session,
    eng->templates,
    eng->template_count,
    &eng->result);
  eng->result.action = action_type;
  Rule_EvaluateResult(&eng->cfg, &eng->session, &eng->result);
}

static uint8_t rule_session_timed_out(const RuleEngine *eng)
{
  if ((eng == NULL) || (eng->session.start_ms == 0U))
  {
    return 0U;
  }

  if (rule_elapsed_ms(eng->session.start_ms, eng->now_ms) >= eng->cfg.action_timeout_ms)
  {
    return 1U;
  }

  return 0U;
}

static AxisIndex rule_find_dominant_axis(const ActionSession *session)
{
  AxisIndex best_axis;
  uint32_t axis;
  float best_amp;

  if (session == NULL)
  {
    return AXIS_UPPER_YAW;
  }

  best_axis = AXIS_UPPER_YAW;
  best_amp = session->amp_abs[(uint32_t)best_axis];

  for (axis = 1U; axis < AXIS_COUNT; axis++)
  {
    if (session->amp_abs[axis] > best_amp)
    {
      best_amp = session->amp_abs[axis];
      best_axis = (AxisIndex)axis;
    }
  }

  return best_axis;
}
