/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "./usart/yuanzi_usart.h"
#include "semphr.h"
#include "string.h"
#include <stdlib.h>   // strtof
#include <ctype.h>    // isdigit
#include <stdint.h>
#include "ESP8266.h"
#include "onenet.h"
#include "debug_uart7.h"
#include "motion_mode.h"
#include "motion_ai.h"
#include "motion_window_test.h"
#include "rule_action_recognizer.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern volatile uint8_t  g_fused_row_ready;
extern volatile uint64_t g_fused_ts_us;
extern volatile float    g_fused_upper_yaw;
extern volatile float    g_fused_upper_pitch;
extern volatile float    g_fused_upper_roll;
extern volatile float    g_fused_fore_yaw;
extern volatile float    g_fused_fore_pitch;
extern volatile float    g_fused_fore_roll;
extern volatile uint32_t g_fused_seq_u;
extern volatile uint32_t g_fused_seq_f;
extern volatile uint32_t g_fused_lost_u;
extern volatile uint32_t g_fused_lost_f;
extern volatile int32_t  g_fused_upper_heart_rate;
extern volatile int32_t  g_fused_upper_spo2;
extern volatile int8_t   g_fused_upper_hr_valid;
extern volatile int8_t   g_fused_upper_spo2_valid;
extern volatile uint32_t g_fused_upper_ppg_fill;
extern volatile uint32_t g_fused_upper_ppg_calc_count;
extern volatile uint32_t g_fused_upper_ppg_pending;
extern volatile uint32_t g_fused_upper_ppg_part_id;
extern volatile uint32_t g_fused_upper_ppg_rev_id;
extern volatile uint32_t g_fused_upper_ppg_int_level;
extern volatile int32_t  g_fused_fore_heart_rate;
extern volatile int32_t  g_fused_fore_spo2;
extern volatile int8_t   g_fused_fore_hr_valid;
extern volatile int8_t   g_fused_fore_spo2_valid;
extern volatile uint32_t g_fused_fore_ppg_fill;
extern volatile uint32_t g_fused_fore_ppg_calc_count;
extern volatile uint32_t g_fused_fore_ppg_pending;
extern volatile uint32_t g_fused_fore_ppg_part_id;
extern volatile uint32_t g_fused_fore_ppg_rev_id;
extern volatile uint32_t g_fused_fore_ppg_int_level;
extern volatile uint32_t g_align_fail_count;
extern volatile uint8_t  g_motion_ai_restart_req;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
extern TIM_HandleTypeDef htim2;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Task1 */
osThreadId_t Task1Handle;
const osThreadAttr_t Task1_attributes = {
  .name = "Task1",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for Task2 */
osThreadId_t Task2Handle;
const osThreadAttr_t Task2_attributes = {
  .name = "Task2",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for myQueue01 */
osMessageQueueId_t myQueue01Handle;
const osMessageQueueAttr_t myQueue01_attributes = {
  .name = "myQueue01"
};
/* Definitions for myBinarySem01 */
osSemaphoreId_t myBinarySem01Handle;
const osSemaphoreAttr_t myBinarySem01_attributes = {
  .name = "myBinarySem01"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

QueueHandle_t Semaphore;
static RuleEngine g_task1_rule_engine;
static uint8_t task1_consume_ai_restart_request(void);
static uint8_t task1_try_take_fused_frame(motion_fused_frame_t *frame);
static uint8_t task1_try_run_model_window_test(void);
static uint8_t task1_mode_uses_single_test(motion_output_mode_t mode);
static void task1_output_state_reset(motion_output_mode_t mode, uint8_t fresh_session);
static void task1_process_fused_frame(const motion_fused_frame_t *frame);
static void task1_output_capture_csv(const motion_fused_frame_t *frame);
static void task1_output_recognition_csv(
  const motion_fused_frame_t *frame,
  const motion_ai_result_t *result);
static void task1_output_brief_result(
  const motion_fused_frame_t *frame,
  const motion_ai_result_t *result);
static void task1_output_rule_debug(
  const motion_fused_frame_t *frame,
  const RuleEngine *eng);
static int32_t task1_bio_value_or_invalid(int32_t value, int8_t valid);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTask1(void *argument);
void StartTask2(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

    Semaphore = xSemaphoreCreateBinary();
    
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of myBinarySem01 */
  myBinarySem01Handle = osSemaphoreNew(1, 1, &myBinarySem01_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of myQueue01 */
  myQueue01Handle = osMessageQueueNew (16, sizeof(uint16_t), &myQueue01_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of Task1 */
  Task1Handle = osThreadNew(StartTask1, NULL, &Task1_attributes);

  /* creation of Task2 */
  Task2Handle = osThreadNew(StartTask2, NULL, &Task2_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
HAL_TIM_Base_Start_IT(&htim2);

  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask1 */
/**
* @brief Function implementing the Task1 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask1 */
void StartTask1(void *argument)
{
  /* USER CODE BEGIN StartTask1 */
  MotionAi_Init();
  MotionWindowTest_Init();
  RuleEngine_Init(&g_task1_rule_engine, NULL);
  MotionAi_SetSingleTestEnabled(task1_mode_uses_single_test(g_motion_output_mode));
  task1_output_state_reset(g_motion_output_mode, 1U);
  /* Infinite loop */
  for(;;)
  {
    motion_fused_frame_t fused_frame;

    if (task1_consume_ai_restart_request())
    {
      MotionAi_SetSingleTestEnabled(task1_mode_uses_single_test(g_motion_output_mode));
      MotionAi_Reset();
      RuleEngine_Init(&g_task1_rule_engine, NULL);
      task1_output_state_reset(g_motion_output_mode, 1U);
      continue;
    }

    if (task1_try_run_model_window_test())
    {
      continue;
    }

    if ((g_motion_output_mode != MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST) &&
        task1_try_take_fused_frame(&fused_frame))
    {
      task1_process_fused_frame(&fused_frame);
    }

    osDelay(10);
  }
  /* USER CODE END StartTask1 */
}

/* USER CODE BEGIN Header_StartTask2 */
/**
* @brief Function implementing the Task2 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask2 */
void StartTask2(void *argument)
{
  /* USER CODE BEGIN StartTask2 */
  uint32_t last_publish_tick = 0U;
  uint8_t subscribe_logged = 0U;

  osDelay(1000);
  Debug_Printf("NET TASK START\r\n");

  for(;;)
  {
    ESP8266_Clear();
    ESP8266_ClearTransportError();
    OneNet_ClearSessionError();
    OneNet_ResetSubscribeState();
    subscribe_logged = 0U;

    while (ESP8266_Init() == 0U)
    {
      Debug_Printf("[TCP] INIT FAIL code=%u\r\n", (unsigned int)ESP8266_GetLastInitStatus());
      ESP8266_Clear();
      ESP8266_ClearTransportError();
      osDelay(1000);
    }
    Debug_Printf("[TCP] INIT OK\r\n");

    while (OneNet_DevLink() == 0U)
    {
      Debug_Printf("[MQTT] CONNECT FAIL code=%u ack=%u\r\n",
                   (unsigned int)OneNet_GetLastStatus(),
                   (unsigned int)OneNet_GetLastConnAckCode());
      if (ESP8266_HasTransportError() != 0U)
      {
        Debug_Printf("[TCP] TRANSPORT ERROR\r\n");
        break;
      }
      osDelay(1000);
    }

    if (ESP8266_HasTransportError() != 0U)
    {
      Debug_Printf("[TCP] RESTART FROM INIT\r\n");
      osDelay(1000);
      continue;
    }
    Debug_Printf("[MQTT] CONNECT OK\r\n");

    if (OneNet_Subscribe() == 0U)
    {
      Debug_Printf("[MQTT] SUBSCRIBE SEND FAIL code=%u topic=%s\r\n",
                   (unsigned int)OneNet_GetLastStatus(),
                   OneNet_GetLastSubscribeTopic());
      ESP8266_Clear();
      ESP8266_ClearTransportError();
      OneNet_ClearSessionError();
      Debug_Printf("[TCP] WIFI REUSE IF CONNECTED, REINIT TCP/MQTT\r\n");
      osDelay(1000);
      continue;
    }
    Debug_Printf("[MQTT] SUBSCRIBE SENT\r\n");

    osDelay(200);
    last_publish_tick = osKernelGetTickCount();

    for (;;)
    {
      uint8_t *ipd_data = NULL;
      uint32_t now = osKernelGetTickCount();
      if ((now - last_publish_tick) >= ONENET_PUBLISH_INTERVAL_MS)
      {
        if (OneNet_Publish() == 0U)
        {
          Debug_Printf("[MQTT] PUBLISH FAIL code=%u\r\n", (unsigned int)OneNet_GetLastStatus());
          break;
        }
        last_publish_tick = now;
      }

      ipd_data = ESP8266_GetIPD(50U);
      if (ipd_data != NULL)
      {
        OneNet_RevPro(ipd_data);
        ESP8266_Clear();
      }

      if ((subscribe_logged == 0U) && (OneNet_IsSubscribeReady() != 0U))
      {
        Debug_Printf("[MQTT] SUBSCRIBE OK\r\n");
        subscribe_logged = 1U;
        last_publish_tick = now;
      }

      if ((ESP8266_HasTransportError() != 0U) || (OneNet_HasSessionError() != 0U))
      {
        if (ESP8266_HasTransportError() != 0U)
        {
          Debug_Printf("[TCP] TRANSPORT ERROR IN LOOP\r\n");
        }
        if (OneNet_HasSessionError() != 0U)
        {
          if (OneNet_GetLastStatus() == ONENET_STATUS_FAIL_SUB_ACK)
          {
            Debug_Printf("[MQTT] SUBSCRIBE FAIL code=%u topic=%s\r\n",
                         (unsigned int)OneNet_GetLastStatus(),
                         OneNet_GetLastSubscribeTopic());
          }
          else
          {
            Debug_Printf("[MQTT] SESSION ERROR code=%u\r\n", (unsigned int)OneNet_GetLastStatus());
          }
        }
        break;
      }

      if (subscribe_logged == 0U)
      {
        osDelay(10);
        continue;
      }

      osDelay(10);
    }

    ESP8266_Clear();
    ESP8266_ClearTransportError();
    OneNet_ClearSessionError();
    Debug_Printf("[TCP] WIFI REUSE IF CONNECTED, REINIT TCP/MQTT\r\n");
    osDelay(1000);
  }
  /* USER CODE END StartTask2 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

typedef struct
{
  motion_output_mode_t last_mode;
  uint8_t capture_header_printed;
  uint8_t recognition_header_printed;
  uint8_t rule_header_printed;
  uint8_t recognition_done_reported;
  uint8_t brief_last_infer_count;
  uint8_t brief_final_reported;
} motion_output_state_t;

volatile motion_output_mode_t g_motion_output_mode =
  MOTION_OUTPUT_MODE_SELECT;
volatile uint8_t g_motion_single_armed = 0U;

static motion_output_state_t g_task1_output_state =
{
  (motion_output_mode_t)0xFF,
  0U,
  0U,
  0U,
  0U,
  0U,
  0U
};

static uint8_t task1_consume_ai_restart_request(void)
{
  uint8_t requested = 0U;

  taskENTER_CRITICAL();
  if (g_motion_ai_restart_req != 0U)
  {
    g_motion_ai_restart_req = 0U;
    g_fused_row_ready = 0U;
    requested = 1U;
  }
  taskEXIT_CRITICAL();

  return requested;
}

static uint8_t task1_mode_uses_single_test(motion_output_mode_t mode)
{
  return (mode == MOTION_OUTPUT_MODE_SINGLE_ONCE) ? 1U : 0U;
}

static uint8_t task1_try_run_model_window_test(void)
{
  if (g_motion_output_mode != MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST)
  {
    return 0U;
  }

  return MotionWindowTest_RunPending();
}

static void task1_output_state_reset(motion_output_mode_t mode, uint8_t fresh_session)
{
  const motion_ai_result_t *result = MotionAi_GetResult();

  g_task1_output_state.last_mode = mode;
  g_task1_output_state.capture_header_printed = 0U;
  g_task1_output_state.recognition_header_printed = 0U;
  g_task1_output_state.rule_header_printed = 0U;

  if (fresh_session != 0U)
  {
    g_task1_output_state.recognition_done_reported = 0U;
    g_task1_output_state.brief_last_infer_count = 0U;
    g_task1_output_state.brief_final_reported = 0U;
    return;
  }

  g_task1_output_state.recognition_done_reported =
    (result->test_done != 0U) ? 1U : 0U;
  g_task1_output_state.brief_last_infer_count = result->infer_count;
  g_task1_output_state.brief_final_reported =
    (result->test_done != 0U) ? 1U : 0U;
}

static uint8_t task1_try_take_fused_frame(motion_fused_frame_t *frame)
{
  uint8_t has_row = 0U;

  if (frame == NULL)
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  if (g_fused_row_ready)
  {
    frame->ts_us = g_fused_ts_us;
    frame->upper_yaw = g_fused_upper_yaw;
    frame->upper_pitch = g_fused_upper_pitch;
    frame->upper_roll = g_fused_upper_roll;
    frame->fore_yaw = g_fused_fore_yaw;
    frame->fore_pitch = g_fused_fore_pitch;
    frame->fore_roll = g_fused_fore_roll;
    frame->seq_u = g_fused_seq_u;
    frame->seq_f = g_fused_seq_f;
    frame->lost_u = g_fused_lost_u;
    frame->lost_f = g_fused_lost_f;
    frame->upper_bio.heart_rate = g_fused_upper_heart_rate;
    frame->upper_bio.spo2 = g_fused_upper_spo2;
    frame->upper_bio.hr_valid = g_fused_upper_hr_valid;
    frame->upper_bio.spo2_valid = g_fused_upper_spo2_valid;
    frame->upper_bio.ppg_fill = g_fused_upper_ppg_fill;
    frame->upper_bio.ppg_calc_count = g_fused_upper_ppg_calc_count;
    frame->upper_bio.ppg_pending = g_fused_upper_ppg_pending;
    frame->upper_bio.ppg_part_id = g_fused_upper_ppg_part_id;
    frame->upper_bio.ppg_rev_id = g_fused_upper_ppg_rev_id;
    frame->upper_bio.ppg_int_level = g_fused_upper_ppg_int_level;
    frame->fore_bio.heart_rate = g_fused_fore_heart_rate;
    frame->fore_bio.spo2 = g_fused_fore_spo2;
    frame->fore_bio.hr_valid = g_fused_fore_hr_valid;
    frame->fore_bio.spo2_valid = g_fused_fore_spo2_valid;
    frame->fore_bio.ppg_fill = g_fused_fore_ppg_fill;
    frame->fore_bio.ppg_calc_count = g_fused_fore_ppg_calc_count;
    frame->fore_bio.ppg_pending = g_fused_fore_ppg_pending;
    frame->fore_bio.ppg_part_id = g_fused_fore_ppg_part_id;
    frame->fore_bio.ppg_rev_id = g_fused_fore_ppg_rev_id;
    frame->fore_bio.ppg_int_level = g_fused_fore_ppg_int_level;
    frame->align_fail_count = g_align_fail_count;
    g_fused_row_ready = 0U;
    has_row = 1U;
  }
  taskEXIT_CRITICAL();

  return has_row;
}

static void task1_process_fused_frame(const motion_fused_frame_t *frame)
{
  const motion_ai_result_t *result = NULL;
  motion_output_mode_t current_mode;
  motion_output_mode_t previous_mode;

  if (frame == NULL)
  {
    return;
  }

  current_mode = g_motion_output_mode;
  previous_mode = g_task1_output_state.last_mode;
  if (g_task1_output_state.last_mode != current_mode)
  {
    MotionAi_SetSingleTestEnabled(task1_mode_uses_single_test(current_mode));

    if ((previous_mode == MOTION_OUTPUT_MODE_RULE_DEBUG) ||
        (current_mode == MOTION_OUTPUT_MODE_RULE_DEBUG))
    {
      RuleEngine_Init(&g_task1_rule_engine, NULL);
    }

    if ((previous_mode != (motion_output_mode_t)0xFF) &&
        (task1_mode_uses_single_test(previous_mode) !=
         task1_mode_uses_single_test(current_mode)))
    {
      MotionAi_Reset();
      task1_output_state_reset(current_mode, 1U);
      return;
    }

    task1_output_state_reset(current_mode, 0U);
  }

  switch (current_mode)
  {
    case MOTION_OUTPUT_MODE_CAPTURE:
      task1_output_capture_csv(frame);
      break;

    case MOTION_OUTPUT_MODE_RECOGNITION_VERBOSE:
      result = MotionAi_ProcessFusedFrame(frame);
      task1_output_recognition_csv(frame, result);
      break;

    case MOTION_OUTPUT_MODE_SINGLE_ONCE:
      if (g_motion_single_armed == 0U)
      {
        return;
      }
      result = MotionAi_ProcessFusedFrame(frame);
      task1_output_recognition_csv(frame, result);
      if ((result != NULL) && (result->test_done != 0U))
      {
        g_motion_single_armed = 0U;
      }
      break;

    case MOTION_OUTPUT_MODE_BIO_AI_BRIEF:
      result = MotionAi_ProcessFusedFrame(frame);
      task1_output_brief_result(frame, result);
      break;

    case MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST:
      break;

    case MOTION_OUTPUT_MODE_RULE_DEBUG:
      {
        float raw[AXIS_COUNT];

        raw[AXIS_UPPER_YAW] = frame->upper_yaw;
        raw[AXIS_UPPER_PITCH] = frame->upper_pitch;
        raw[AXIS_UPPER_ROLL] = frame->upper_roll;
        raw[AXIS_FORE_YAW] = frame->fore_yaw;
        raw[AXIS_FORE_PITCH] = frame->fore_pitch;
        raw[AXIS_FORE_ROLL] = frame->fore_roll;

        RuleEngine_ProcessRaw(
          &g_task1_rule_engine,
          raw,
          (uint32_t)(frame->ts_us / 1000ULL));
        task1_output_rule_debug(frame, &g_task1_rule_engine);
      }
      break;

    default:
      task1_output_capture_csv(frame);
      break;
  }
}

static void task1_output_capture_csv(const motion_fused_frame_t *frame)
{
  int32_t upper_hr;
  int32_t upper_spo2;

  if (frame == NULL)
  {
    return;
  }

  upper_hr = task1_bio_value_or_invalid(
    frame->upper_bio.heart_rate,
    frame->upper_bio.hr_valid);
  upper_spo2 = task1_bio_value_or_invalid(
    frame->upper_bio.spo2,
    frame->upper_bio.spo2_valid);

  if (g_task1_output_state.capture_header_printed == 0U)
  {
    printf("ts_ms,upper_yaw,upper_pitch,upper_roll,fore_yaw,fore_pitch,fore_roll,hr,hr_valid,spo2,spo2_valid,ppg_fill,ppg_calc_count,ppg_pending,lost_u,lost_f,align_fail_count\r\n");
    g_task1_output_state.capture_header_printed = 1U;
  }

  printf("%llu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%ld,%d,%ld,%d,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
         (unsigned long long)(frame->ts_us / 1000ULL),
         frame->upper_yaw,
         frame->upper_pitch,
         frame->upper_roll,
         frame->fore_yaw,
         frame->fore_pitch,
         frame->fore_roll,
         (long)upper_hr,
         (int)frame->upper_bio.hr_valid,
         (long)upper_spo2,
         (int)frame->upper_bio.spo2_valid,
         (unsigned long)frame->upper_bio.ppg_fill,
         (unsigned long)frame->upper_bio.ppg_calc_count,
         (unsigned long)frame->upper_bio.ppg_pending,
         (unsigned long)frame->lost_u,
         (unsigned long)frame->lost_f,
         (unsigned long)frame->align_fail_count);
}

static void task1_output_recognition_csv(
  const motion_fused_frame_t *frame,
  const motion_ai_result_t *result)
{
  if ((frame == NULL) || (result == NULL))
  {
    return;
  }

  if (result->test_done == 0U)
  {
    g_task1_output_state.recognition_done_reported = 0U;
  }
  else if (g_task1_output_state.recognition_done_reported != 0U)
  {
    return;
  }

  if (g_task1_output_state.recognition_header_printed == 0U)
  {
    printf("ts_ms,ai_state,infer_count,smooth_ready,test_done,motion_energy,latest_label,latest_prob,top1_label,top1_prob_avg,final_label,abnormal_flag,align_fail_total,align_fail_delta,p_rest,p_elbow_flex,p_front_raise,p_side_raise,p_shoulder_raise\r\n");
    g_task1_output_state.recognition_header_printed = 1U;
  }

  printf("%llu,%s,%u,%u,%u,%.4f,%s,%.4f,%s,%.4f,%s,%u,%lu,%lu,%.4f,%.4f,%.4f,%.4f,%.4f\r\n",
         (unsigned long long)(frame->ts_us / 1000ULL),
         MotionAi_StateName(result->ai_state),
         (unsigned int)result->infer_count,
         (unsigned int)result->smooth_ready,
         (unsigned int)result->test_done,
         result->motion_energy,
         MotionAi_LabelName(result->latest_label),
         result->latest_prob,
         MotionAi_LabelName(result->top1_label),
         result->top1_prob_avg,
         MotionAi_LabelName(result->final_label),
         (unsigned int)result->abnormal_flag,
         (unsigned long)result->align_fail_total,
         (unsigned long)result->align_fail_delta,
         result->probs[0],
         result->probs[1],
         result->probs[2],
         result->probs[3],
         result->probs[4]);

  if (result->test_done != 0U)
  {
    printf("TEST_DONE,stable_label=%s,stable_prob=%.4f,latest_label=%s,latest_prob=%.4f,abnormal_flag=%u\r\n",
           MotionAi_LabelName(result->final_label),
           result->top1_prob_avg,
           MotionAi_LabelName(result->latest_label),
           result->latest_prob,
           (unsigned int)result->abnormal_flag);
    g_task1_output_state.recognition_done_reported = 1U;
  }
}

static void task1_output_brief_result(
  const motion_fused_frame_t *frame,
  const motion_ai_result_t *result)
{
  int32_t upper_hr;
  int32_t upper_spo2;
  int32_t fore_hr;
  int32_t fore_spo2;

  if ((frame == NULL) || (result == NULL))
  {
    return;
  }

  upper_hr = task1_bio_value_or_invalid(
    frame->upper_bio.heart_rate,
    frame->upper_bio.hr_valid);
  upper_spo2 = task1_bio_value_or_invalid(
    frame->upper_bio.spo2,
    frame->upper_bio.spo2_valid);
  fore_hr = task1_bio_value_or_invalid(
    frame->fore_bio.heart_rate,
    frame->fore_bio.hr_valid);
  fore_spo2 = task1_bio_value_or_invalid(
    frame->fore_bio.spo2,
    frame->fore_bio.spo2_valid);

  if ((result->infer_count != 0U) &&
      (result->infer_count != g_task1_output_state.brief_last_infer_count))
  {
    printf("BRIEF,u_seq=%lu,f_seq=%lu,u_hr=%ld,u_spo2=%ld,f_hr=%ld,f_spo2=%ld,latest_label=%s\r\n",
           (unsigned long)frame->seq_u,
           (unsigned long)frame->seq_f,
           (long)upper_hr,
           (long)upper_spo2,
           (long)fore_hr,
           (long)fore_spo2,
           MotionAi_LabelName(result->latest_label));
  }

  g_task1_output_state.brief_last_infer_count = result->infer_count;

  if ((result->test_done != 0U) &&
      (g_task1_output_state.brief_final_reported == 0U))
  {
    printf("BRIEF_FINAL,u_seq=%lu,f_seq=%lu,u_hr=%ld,u_spo2=%ld,f_hr=%ld,f_spo2=%ld,latest_label=%s,final_label=%s\r\n",
           (unsigned long)frame->seq_u,
           (unsigned long)frame->seq_f,
           (long)upper_hr,
           (long)upper_spo2,
           (long)fore_hr,
           (long)fore_spo2,
           MotionAi_LabelName(result->latest_label),
           MotionAi_LabelName(result->final_label));
    g_task1_output_state.brief_final_reported = 1U;
  }
}

static void task1_output_rule_debug(
  const motion_fused_frame_t *frame,
  const RuleEngine *eng)
{
  const ActionSession *session;
  const ActionResult *latched_result;
  const ActionResult *display_result;
  const ActionTemplate *templates;
  ActionResult preview_result;
  uint32_t template_count;
  uint32_t idx;

  if ((frame == NULL) || (eng == NULL))
  {
    return;
  }

  session = RuleEngine_GetSession(eng);
  latched_result = RuleEngine_GetResult(eng);
  template_count = eng->template_count;
  templates = eng->templates;

  if ((templates == NULL) || (template_count == 0U))
  {
    templates = Rule_GetDefaultTemplates(&template_count);
  }

  memset(&preview_result, 0, sizeof(preview_result));
  (void)Rule_RecognizeAction(session, templates, template_count, &preview_result);
  Rule_EvaluateResult(&eng->cfg, session, &preview_result);

  if ((latched_result != NULL) && (latched_result->valid != 0U))
  {
    display_result = latched_result;
  }
  else
  {
    display_result = &preview_result;
  }

  if (g_task1_output_state.rule_header_printed == 0U)
  {
    printf("ts_ms,state,motion_energy,baseline_valid,session_active,peak_count,preview_action,final_action,matched_template,match_score,complete,timed_out,complete_score,amp_score,peak_score,total_score,grade,primary_axis,primary_axis_amp,peak_hold_ms,total_time_ms,amp_upper_yaw,amp_upper_pitch,amp_upper_roll,amp_fore_yaw,amp_fore_pitch,amp_fore_roll,peak_upper_yaw,peak_upper_pitch,peak_upper_roll,peak_fore_yaw,peak_fore_pitch,peak_fore_roll");
    for (idx = 0U; idx < preview_result.template_debug_count; idx++)
    {
      printf(",pass_%s,dist_%s",
             preview_result.template_debug[idx].name,
             preview_result.template_debug[idx].name);
    }
    printf("\r\n");
    g_task1_output_state.rule_header_printed = 1U;
  }

  printf("%llu,%s,%.4f,%u,%u,%lu,%s,%s,%s,%.4f,%u,%u,%u,%u,%u,%u,%s,%s,%.4f,%lu,%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
         (unsigned long long)(frame->ts_us / 1000ULL),
         Rule_StateName(eng->state),
         eng->motion_energy,
         (unsigned int)eng->baseline_valid,
         (unsigned int)((session != NULL) ? session->active : 0U),
         (unsigned long)((session != NULL) ? session->peak_count : 0U),
         Rule_ActionName(preview_result.action),
         Rule_ActionName(display_result->action),
         (display_result->matched_template_name != NULL) ? display_result->matched_template_name : "unknown",
         (display_result->matched_template != NULL) ? display_result->match_score : -1.0f,
         (unsigned int)display_result->complete,
         (unsigned int)display_result->timed_out,
         (unsigned int)display_result->completeness_score,
         (unsigned int)display_result->amplitude_score,
         (unsigned int)display_result->peak_hold_score,
         (unsigned int)display_result->score,
         Rule_GradeName(display_result->grade),
         Rule_AxisName(display_result->primary_axis),
         display_result->primary_axis_amp,
         (unsigned long)display_result->peak_hold_ms,
         (unsigned long)display_result->total_time_ms,
         (session != NULL) ? session->amp[AXIS_UPPER_YAW] : 0.0f,
         (session != NULL) ? session->amp[AXIS_UPPER_PITCH] : 0.0f,
         (session != NULL) ? session->amp[AXIS_UPPER_ROLL] : 0.0f,
         (session != NULL) ? session->amp[AXIS_FORE_YAW] : 0.0f,
         (session != NULL) ? session->amp[AXIS_FORE_PITCH] : 0.0f,
         (session != NULL) ? session->amp[AXIS_FORE_ROLL] : 0.0f,
         (session != NULL) ? session->peak_mean[AXIS_UPPER_YAW] : 0.0f,
         (session != NULL) ? session->peak_mean[AXIS_UPPER_PITCH] : 0.0f,
         (session != NULL) ? session->peak_mean[AXIS_UPPER_ROLL] : 0.0f,
         (session != NULL) ? session->peak_mean[AXIS_FORE_YAW] : 0.0f,
         (session != NULL) ? session->peak_mean[AXIS_FORE_PITCH] : 0.0f,
         (session != NULL) ? session->peak_mean[AXIS_FORE_ROLL] : 0.0f);

  for (idx = 0U; idx < display_result->template_debug_count; idx++)
  {
    float distance_value;

    distance_value = (display_result->template_debug[idx].filter_passed != 0U) ?
      display_result->template_debug[idx].distance_score : -1.0f;

    printf(",%u,%.3f",
           (unsigned int)display_result->template_debug[idx].filter_passed,
           distance_value);
  }

  printf("\r\n");
}

static int32_t task1_bio_value_or_invalid(int32_t value, int8_t valid)
{
  if (valid == 0)
  {
    return -999;
  }

  return value;
}

/* USER CODE END Application */

