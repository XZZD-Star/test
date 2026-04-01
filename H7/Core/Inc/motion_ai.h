#ifndef MOTION_AI_H
#define MOTION_AI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "app_x-cube-ai.h"

#define MOTION_AI_FEATURE_COUNT          APP_X_CUBE_AI_INPUT_FEATURES
#define MOTION_AI_WINDOW_FRAMES          APP_X_CUBE_AI_INPUT_FRAMES
#define MOTION_AI_CLASS_COUNT            APP_X_CUBE_AI_OUTPUT_CLASSES

#define MOTION_AI_BASELINE_TARGET_FRAMES (12U)
#define MOTION_AI_BASELINE_MAX_FRAMES    (15U)
#define MOTION_AI_INFER_STRIDE           (5U)
#define MOTION_AI_SMOOTHING_WINDOW       (4U)
#define MOTION_AI_SINGLE_TEST_MODE_DEFAULT (0U)

#define MOTION_AI_P_KNOWN_TH             (0.70f)
#define MOTION_AI_E_REST_TH              (0.0f)
#define MOTION_AI_ABNORMAL_LATCH_FRAMES  (10U)

#if (MOTION_AI_BASELINE_TARGET_FRAMES > MOTION_AI_BASELINE_MAX_FRAMES)
#error "MOTION_AI_BASELINE_TARGET_FRAMES must be <= MOTION_AI_BASELINE_MAX_FRAMES"
#endif

typedef struct
{
  int32_t heart_rate;
  int32_t spo2;
  int8_t hr_valid;
  int8_t spo2_valid;
  uint32_t ppg_fill;
  uint32_t ppg_calc_count;
  uint32_t ppg_pending;
  uint32_t ppg_part_id;
  uint32_t ppg_rev_id;
  uint32_t ppg_int_level;
} motion_bio_sample_t;

typedef struct
{
  uint64_t ts_us;
  float upper_yaw;
  float upper_pitch;
  float upper_roll;
  float fore_yaw;
  float fore_pitch;
  float fore_roll;
  uint32_t seq_u;
  uint32_t seq_f;
  uint32_t lost_u;
  uint32_t lost_f;
  motion_bio_sample_t upper_bio;
  motion_bio_sample_t fore_bio;
  uint32_t align_fail_count;
} motion_fused_frame_t;

typedef enum
{
  MOTION_AI_STATE_WAIT_START = 0,
  MOTION_AI_STATE_COLLECT_BASELINE,
  MOTION_AI_STATE_FILL_WINDOW,
  MOTION_AI_STATE_RUNNING,
  MOTION_AI_STATE_TEST_DONE
} motion_ai_state_t;

typedef enum
{
  MOTION_LABEL_REST = 0,
  MOTION_LABEL_ELBOW_FLEX,
  MOTION_LABEL_FRONT_RAISE,
  MOTION_LABEL_SIDE_RAISE,
  MOTION_LABEL_SHOULDER_RAISE,
  MOTION_LABEL_UNKNOWN
} motion_label_t;

typedef struct
{
  motion_ai_state_t ai_state;
  motion_label_t top1_label;
  motion_label_t final_label;
  motion_label_t latest_label;
  float base_mean[MOTION_AI_FEATURE_COUNT];
  float delta[MOTION_AI_FEATURE_COUNT];
  float probs[MOTION_AI_CLASS_COUNT];
  float latest_prob;
  float top1_prob_avg;
  float motion_energy;
  uint8_t abnormal_flag;
  uint8_t smooth_ready;
  uint8_t test_done;
  uint8_t infer_count;
  uint32_t align_fail_total;
  uint32_t align_fail_delta;
  uint16_t baseline_count;
  uint16_t window_count;
} motion_ai_result_t;

void MotionAi_Init(void);
void MotionAi_Reset(void);
void MotionAi_SetSingleTestEnabled(uint8_t enabled);
const motion_ai_result_t* MotionAi_ProcessFusedFrame(const motion_fused_frame_t *frame);
const motion_ai_result_t* MotionAi_GetResult(void);
const char* MotionAi_StateName(motion_ai_state_t state);
const char* MotionAi_LabelName(motion_label_t label);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_AI_H */
