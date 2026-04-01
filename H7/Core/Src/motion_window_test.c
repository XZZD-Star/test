#include "motion_window_test.h"

#include <stdio.h>

#include "main.h"
#include "app_x-cube-ai.h"

#if (APP_X_CUBE_AI_INPUT_FRAMES != 30U)
#error "Motion window test expects a 30-frame model input."
#endif

#if (APP_X_CUBE_AI_INPUT_FEATURES != 6U)
#error "Motion window test expects 6 features per frame."
#endif

#if (APP_X_CUBE_AI_OUTPUT_CLASSES != 5U)
#error "Motion window test expects 5 output classes."
#endif

static volatile uint8_t g_window_test_run_pending = 0U;

/*
 * Replace the fixed 30x6 window below before flashing.
 * Per-row feature order must stay:
 * upper_yaw, upper_pitch, upper_roll, fore_yaw, fore_pitch, fore_roll
 */
 
//static const float g_window_test_input[30][6] = {
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
//  { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }
//};


static const float g_window_test_input[30][6] = {
  { -89.42f, 76.38f, 54.22f, -51.27f, 78.20f, 29.90f },
  { -89.66f, 83.36f, 51.75f, -51.57f, 85.28f, 29.02f },
  { -89.92f, 88.48f, 50.20f, -51.86f, 90.56f, 28.90f },
  { -90.28f, 95.37f, 47.70f, -52.33f, 97.77f, 26.99f },
  { -90.66f, 101.71f, 44.72f, -52.97f, 104.52f, 24.80f },
  { -91.13f, 108.94f, 41.19f, -53.97f, 112.25f, 22.66f },
  { -91.93f, 116.90f, 37.49f, -55.29f, 120.78f, 21.76f },
  { -93.17f, 124.80f, 33.67f, -57.47f, 129.08f, 20.70f },
  { -94.36f, 132.08f, 29.97f, -61.07f, 136.66f, 18.42f },
  { -94.98f, 138.56f, 27.26f, -66.01f, 143.41f, 16.42f },
  { -95.81f, 144.17f, 24.85f, -72.12f, 149.17f, 13.16f },
  { -96.99f, 148.71f, 21.96f, -79.22f, 153.64f, 7.72f },
  { -98.18f, 152.30f, 19.35f, -87.09f, 156.91f, 0.18f },
  { -99.06f, 154.97f, 17.51f, -95.66f, 159.28f, -7.96f },
  { -98.71f, 156.18f, 17.37f, -99.82f, 160.55f, -11.56f },
  { -97.68f, 155.59f, 18.61f, -94.71f, 160.21f, -7.22f },
  { -96.61f, 153.34f, 20.68f, -83.37f, 158.15f, 2.27f },
  { -95.62f, 149.47f, 23.30f, -72.05f, 154.00f, 10.74f },
  { -94.02f, 144.00f, 26.87f, -64.88f, 147.85f, 15.24f },
  { -92.28f, 137.18f, 30.81f, -60.14f, 140.20f, 17.62f },
  { -91.30f, 129.34f, 34.59f, -56.09f, 131.49f, 18.60f },
  { -90.58f, 121.15f, 38.95f, -52.53f, 122.55f, 21.16f },
  { -90.01f, 112.94f, 43.36f, -49.99f, 113.47f, 22.52f },
  { -89.65f, 104.60f, 47.01f, -48.84f, 104.55f, 23.62f },
  { -89.57f, 96.46f, 50.26f, -48.49f, 95.93f, 23.64f },
  { -89.61f, 89.11f, 53.25f, -48.49f, 88.38f, 23.12f },
  { -89.60f, 83.17f, 56.07f, -48.78f, 82.52f, 25.54f },
  { -89.54f, 77.00f, 58.47f, -49.00f, 75.91f, 27.92f },
  { -89.06f, 69.02f, 61.49f, -48.36f, 66.83f, 29.52f },
  { -87.95f, 58.38f, 65.32f, -47.25f, 55.10f, 30.88f }
};

static uint8_t motion_window_test_try_consume_request(void);
static void motion_window_test_print_output(
  const float output[APP_X_CUBE_AI_OUTPUT_CLASSES]);

void MotionWindowTest_Init(void)
{
  MotionWindowTest_Reset();
}

void MotionWindowTest_Reset(void)
{
  g_window_test_run_pending = 0U;
}

void MotionWindowTest_RequestRun(void)
{
  g_window_test_run_pending = 1U;
}

uint8_t MotionWindowTest_RunPending(void)
{
  float output[APP_X_CUBE_AI_OUTPUT_CLASSES] = {0.0f};

  if (motion_window_test_try_consume_request() == 0U)
  {
    return 0U;
  }

  (void)AppXCubeAI_Run(&g_window_test_input[0][0], output);
  motion_window_test_print_output(output);
  return 1U;
}

static uint8_t motion_window_test_try_consume_request(void)
{
  uint32_t primask = __get_PRIMASK();
  uint8_t pending = 0U;

  __disable_irq();
  pending = g_window_test_run_pending;
  g_window_test_run_pending = 0U;
  if (primask == 0U)
  {
    __enable_irq();
  }

  return pending;
}

static void motion_window_test_print_output(
  const float output[APP_X_CUBE_AI_OUTPUT_CLASSES])
{
  printf("p_rest,p_elbow_flex,p_front_raise,p_side_raise,p_shoulder_raise\r\n");
  printf("%.6f,%.6f,%.6f,%.6f,%.6f\r\n",
         output[0],
         output[1],
         output[2],
         output[3],
         output[4]);
}
