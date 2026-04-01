#ifndef MOTION_MODE_H
#define MOTION_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  MOTION_OUTPUT_MODE_CAPTURE = 0,
  MOTION_OUTPUT_MODE_RECOGNITION_VERBOSE,
  MOTION_OUTPUT_MODE_SINGLE_ONCE,
  MOTION_OUTPUT_MODE_BIO_AI_BRIEF,
  MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST,
  MOTION_OUTPUT_MODE_RULE_DEBUG
} motion_output_mode_t;

#define MOTION_OUTPUT_MODE_SELECT MOTION_OUTPUT_MODE_RULE_DEBUG

extern volatile motion_output_mode_t g_motion_output_mode;
extern volatile uint8_t g_motion_single_armed;

#ifdef __cplusplus
}
#endif

#endif /* MOTION_MODE_H */
