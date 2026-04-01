/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "main.h"
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "./usart/yuanzi_usart.h"
#include "string.h"
#include <stdlib.h>   // strtof
#include <ctype.h>    // isdigit
#include <stdint.h>
#include "usart.h"
#include "motion_mode.h"
#include "motion_window_test.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
typedef struct
{
    uint8_t  sensor_id;
    uint32_t seq;
    float    yaw;
    float    pitch;
    float    roll;
    int32_t  heart_rate;
    int32_t  spo2;
    int8_t   hr_valid;
    int8_t   spo2_valid;
    uint32_t ppg_fill;
    uint32_t ppg_calc_count;
    uint32_t ppg_pending;
    uint32_t ppg_part_id;
    uint32_t ppg_rev_id;
    uint32_t ppg_int_level;
    uint64_t ts_us;
    uint8_t  valid;
} pose_frame_t;

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SENSOR_ID_UPPER    0U
#define SENSOR_ID_FORE     1U
#define ALIGN_THRESHOLD_US 50000ULL

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static uint64_t get_ts_us(void);
static int parse_pose_frame(const uint8_t *buf, uint16_t len, uint8_t default_sensor_id, pose_frame_t *out);
static void update_seq_stats(uint8_t sensor_id, uint32_t seq);
static void process_pose_packet(const uint8_t *buf, uint16_t len, uint8_t default_sensor_id);
static uint64_t abs_diff_u64(uint64_t a, uint64_t b);
static void try_emit_fused(void);
static void reset_pose_pipeline(void);
static void motion_uart4_handle_command(const uint8_t *buf, uint16_t len);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim2;
extern DMA_HandleTypeDef hdma_uart4_rx;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart6;
extern UART_HandleTypeDef huart7;
extern TIM_HandleTypeDef htim1;

/* USER CODE BEGIN EV */
float yaw,pitch,roll;
float yaw2,pitch2,roll2;
float Calibrate_yaw,Calibrate_pitch,Calibrate_roll,Calibrate_yaw2,Calibrate_pitch2,Calibrate_roll2;
int extract_ypr(const uint8_t *buf, int len,
                float *yaw, float *pitch, float *roll);
uint16_t Calibrate_count = 0;     //校准3s计时
uint16_t startRcv = 0;            //接收校准信号标识位

static pose_frame_t upper_frame = {0};
static pose_frame_t fore_frame = {0};

static uint8_t  has_last_seq_u = 0U;
static uint8_t  has_last_seq_f = 0U;
static uint32_t last_seq_u = 0U;
static uint32_t last_seq_f = 0U;
volatile uint32_t g_lost_u = 0U;
volatile uint32_t g_lost_f = 0U;
static uint32_t disorder_u = 0U;
static uint32_t disorder_f = 0U;
volatile uint32_t g_align_fail_count = 0U;

static uint8_t  dwt_ts_inited = 0U;
static uint32_t dwt_cycles_per_us = 1U;
static uint32_t dwt_last_cyccnt = 0U;
static uint64_t dwt_cycle_high = 0U;

volatile uint8_t  g_fused_row_ready = 0U;
volatile uint64_t g_fused_ts_us = 0ULL;
volatile float    g_fused_upper_yaw = 0.0f;
volatile float    g_fused_upper_pitch = 0.0f;
volatile float    g_fused_upper_roll = 0.0f;
volatile float    g_fused_fore_yaw = 0.0f;
volatile float    g_fused_fore_pitch = 0.0f;
volatile float    g_fused_fore_roll = 0.0f;
volatile uint32_t g_fused_seq_u = 0U;
volatile uint32_t g_fused_seq_f = 0U;
volatile uint32_t g_fused_lost_u = 0U;
volatile uint32_t g_fused_lost_f = 0U;
volatile int32_t  g_fused_upper_heart_rate = 0;
volatile int32_t  g_fused_upper_spo2 = 0;
volatile int8_t   g_fused_upper_hr_valid = 0;
volatile int8_t   g_fused_upper_spo2_valid = 0;
volatile uint32_t g_fused_upper_ppg_fill = 0U;
volatile uint32_t g_fused_upper_ppg_calc_count = 0U;
volatile uint32_t g_fused_upper_ppg_pending = 0U;
volatile uint32_t g_fused_upper_ppg_part_id = 0U;
volatile uint32_t g_fused_upper_ppg_rev_id = 0U;
volatile uint32_t g_fused_upper_ppg_int_level = 0U;
volatile int32_t  g_fused_fore_heart_rate = 0;
volatile int32_t  g_fused_fore_spo2 = 0;
volatile int8_t   g_fused_fore_hr_valid = 0;
volatile int8_t   g_fused_fore_spo2_valid = 0;
volatile uint32_t g_fused_fore_ppg_fill = 0U;
volatile uint32_t g_fused_fore_ppg_calc_count = 0U;
volatile uint32_t g_fused_fore_ppg_pending = 0U;
volatile uint32_t g_fused_fore_ppg_part_id = 0U;
volatile uint32_t g_fused_fore_ppg_rev_id = 0U;
volatile uint32_t g_fused_fore_ppg_int_level = 0U;
volatile uint8_t  g_motion_ai_restart_req = 0U;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 stream0 global interrupt.
  */
void DMA1_Stream0_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream0_IRQn 0 */

  /* USER CODE END DMA1_Stream0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_rx);
  /* USER CODE BEGIN DMA1_Stream0_IRQn 1 */

  /* USER CODE END DMA1_Stream0_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream1 global interrupt.
  */
void DMA1_Stream1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream1_IRQn 0 */

  /* USER CODE END DMA1_Stream1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart3_rx);
  /* USER CODE BEGIN DMA1_Stream1_IRQn 1 */

  /* USER CODE END DMA1_Stream1_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream2 global interrupt.
  */
void DMA1_Stream2_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream2_IRQn 0 */

  /* USER CODE END DMA1_Stream2_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_uart4_rx);
  /* USER CODE BEGIN DMA1_Stream2_IRQn 1 */

  /* USER CODE END DMA1_Stream2_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream3 global interrupt.
  */
void DMA1_Stream3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream3_IRQn 0 */

  /* USER CODE END DMA1_Stream3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart6_rx);
  /* USER CODE BEGIN DMA1_Stream3_IRQn 1 */

  /* USER CODE END DMA1_Stream3_IRQn 1 */
}

/**
  * @brief This function handles TIM1 update interrupt.
  */
void TIM1_UP_IRQHandler(void)
{
  /* USER CODE BEGIN TIM1_UP_IRQn 0 */
    
  /* USER CODE END TIM1_UP_IRQn 0 */
  HAL_TIM_IRQHandler(&htim1);
  /* USER CODE BEGIN TIM1_UP_IRQn 1 */

  /* USER CODE END TIM1_UP_IRQn 1 */
}

/**
  * @brief This function handles TIM2 global interrupt.
  */
void TIM2_IRQHandler(void)
{
  /* USER CODE BEGIN TIM2_IRQn 0 */
    /* Calibration disabled for data collection. */
    /*
    if (startRcv)
    {
        Calibrate_count++;
        if (Calibrate_count >= 30)
        {
            Calibrate_yaw = yaw;
            Calibrate_pitch = pitch;
            Calibrate_roll = roll;
            Calibrate_yaw2 = yaw2;
            Calibrate_pitch2 = pitch2;
            Calibrate_roll2 = roll2;
            Calibrate_count = 0;
            startRcv = 0;
        }
    }
    */
  /* USER CODE END TIM2_IRQn 0 */
  HAL_TIM_IRQHandler(&htim2);
  /* USER CODE BEGIN TIM2_IRQn 1 */

  /* USER CODE END TIM2_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */
  uint32_t tmp_flag = 0;
  uint16_t rx_len = 0;
  tmp_flag = __HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE);
  if ((tmp_flag != RESET))
  {
    rx_len = (uint16_t)(RXBUFFERSIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx));
    if (rx_len > RXBUFFERSIZE)
    {
      rx_len = RXBUFFERSIZE;
    }
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    HAL_UART_DMAStop(&huart1);
    process_pose_packet(g_rx_buffer, rx_len, SENSOR_ID_UPPER);
    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    recv_end_flag = 1;
  }

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_rx_buffer, RXBUFFERSIZE);
  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles USART2 global interrupt.
  */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */

  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */

  /* USER CODE END USART2_IRQn 1 */
}

/**
  * @brief This function handles USART3 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_IRQn 0 */
  uint32_t tmp_flag = 0;
  uint16_t rx_len = 0;
  tmp_flag = __HAL_UART_GET_FLAG(&huart3, UART_FLAG_IDLE);
  if ((tmp_flag != RESET))
  {
    rx_len = (uint16_t)(RXBUFFERSIZE - __HAL_DMA_GET_COUNTER(&hdma_usart3_rx));
    if (rx_len > RXBUFFERSIZE)
    {
      rx_len = RXBUFFERSIZE;
    }
    __HAL_UART_CLEAR_IDLEFLAG(&huart3);
    HAL_UART_DMAStop(&huart3);
    process_pose_packet(g_rx_buffer2, rx_len, SENSOR_ID_FORE);
    memset(g_rx_buffer2, 0, sizeof(g_rx_buffer2));
    recv_end_flag2 = 1;
  }
  /* USER CODE END USART3_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  /* USER CODE BEGIN USART3_IRQn 1 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_rx_buffer2, RXBUFFERSIZE);
  /* USER CODE END USART3_IRQn 1 */
}

/**
  * @brief This function handles UART4 global interrupt.
  */
void UART4_IRQHandler(void)
{
  /* USER CODE BEGIN UART4_IRQn 0 */
    uint32_t tmp_flag = 0;
    uint16_t rx_len = 0;
   	tmp_flag =__HAL_UART_GET_FLAG(&huart4,UART_FLAG_IDLE); //��ȡIDLE��־λ
	if((tmp_flag != RESET))//idle��־����λ
	{ 
		__HAL_UART_CLEAR_IDLEFLAG(&huart4);//�����־λ
		//temp = huart1.Instance->SR;  //���״̬�Ĵ���SR,��ȡSR�Ĵ�������ʵ�����SR�Ĵ����Ĺ���
		//temp = huart1.Instance->DR; //��ȡ���ݼĴ����е�����
		//������������Ǿ��Ч
        rx_len = (uint16_t)(RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_uart4_rx));
        if (rx_len > RX_BUFFER_SIZE)
        {
            rx_len = RX_BUFFER_SIZE;
        }
        HAL_UART_DMAStop(&huart4);
        motion_uart4_handle_command(rx_buffer, rx_len);
        memset(rx_buffer, 0, sizeof(rx_buffer));
//        rx_index = sizeof(rx_buffer) - __HAL_DMA_GET_COUNTER(huart4.hdmarx);
         
		//temp  = hdma_usart1_rx.Instance->NDTR;//��ȡNDTR�Ĵ��� ��ȡDMA��δ��������ݸ�����
		//���������Ǿ��Ч
        
//        
    }
  /* USER CODE END UART4_IRQn 0 */
  HAL_UART_IRQHandler(&huart4);
  /* USER CODE BEGIN UART4_IRQn 1 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart4,rx_buffer,RX_BUFFER_SIZE);
  /* USER CODE END UART4_IRQn 1 */
}

/**
  * @brief This function handles USART6 global interrupt.
  */
void USART6_IRQHandler(void)
{
  /* USER CODE BEGIN USART6_IRQn 0 */

  /* USER CODE END USART6_IRQn 0 */
  HAL_UART_IRQHandler(&huart6);
  /* USER CODE BEGIN USART6_IRQn 1 */

  /* USER CODE END USART6_IRQn 1 */
}

/**
  * @brief This function handles UART7 global interrupt.
  */
void UART7_IRQHandler(void)
{
  /* USER CODE BEGIN UART7_IRQn 0 */

  /* USER CODE END UART7_IRQn 0 */
  HAL_UART_IRQHandler(&huart7);
  /* USER CODE BEGIN UART7_IRQn 1 */

  /* USER CODE END UART7_IRQn 1 */
}

/* USER CODE BEGIN 1 */
static char *trim_spaces(char *s)
{
    char *end = NULL;
    while (*s != '\0' && isspace((unsigned char)*s))
    {
        s++;
    }
    if (*s == '\0')
    {
        return s;
    }
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }
    return s;
}

static int parse_u32_token(char *token, uint32_t *out)
{
    char *endptr = NULL;
    unsigned long value = 0UL;
    token = trim_spaces(token);
    if (*token == '\0')
    {
        return 0;
    }
    value = strtoul(token, &endptr, 10);
    endptr = trim_spaces(endptr);
    if (*endptr != '\0')
    {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int parse_i32_token(char *token, int32_t *out)
{
    char *endptr = NULL;
    long value = 0L;

    token = trim_spaces(token);
    if (*token == '\0')
    {
        return 0;
    }

    value = strtol(token, &endptr, 10);
    endptr = trim_spaces(endptr);
    if (*endptr != '\0')
    {
        return 0;
    }

    *out = (int32_t)value;
    return 1;
}

static int parse_float_token(char *token, float *out)
{
    char *endptr = NULL;
    float value = 0.0f;
    token = trim_spaces(token);
    if (*token == '\0')
    {
        return 0;
    }
    value = strtof(token, &endptr);
    endptr = trim_spaces(endptr);
    if (*endptr != '\0')
    {
        return 0;
    }
    *out = value;
    return 1;
}

static uint64_t abs_diff_u64(uint64_t a, uint64_t b)
{
    return (a >= b) ? (a - b) : (b - a);
}

static uint64_t get_ts_us(void)
{
    uint32_t now = 0U;

    if (!dwt_ts_inited)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        dwt_cycles_per_us = HAL_RCC_GetSysClockFreq() / 1000000U;
        if (dwt_cycles_per_us == 0U)
        {
            dwt_cycles_per_us = 1U;
        }
        dwt_last_cyccnt = 0U;
        dwt_cycle_high = 0ULL;
        dwt_ts_inited = 1U;
    }

    now = DWT->CYCCNT;
    if (now < dwt_last_cyccnt)
    {
        dwt_cycle_high += (1ULL << 32);
    }
    dwt_last_cyccnt = now;
    return (dwt_cycle_high + (uint64_t)now) / (uint64_t)dwt_cycles_per_us;
}

static int parse_pose_frame(const uint8_t *buf, uint16_t len, uint8_t default_sensor_id, pose_frame_t *out)
{
    char line[192] = {0};
    char *tokens[15] = {0};
    uint16_t i = 0U;
    uint16_t n = 0U;
    char *cursor = NULL;
    uint16_t token_count = 0U;
    uint32_t sid = default_sensor_id;
    uint32_t seq = 0U;
    float y = 0.0f;
    float p = 0.0f;
    float r = 0.0f;
    int32_t heart_rate = -999;
    int32_t spo2 = -999;
    int32_t hr_valid = 0;
    int32_t spo2_valid = 0;
    uint32_t ppg_fill = 0U;
    uint32_t ppg_calc_count = 0U;
    uint32_t ppg_pending = 0U;
    uint32_t ppg_part_id = 0U;
    uint32_t ppg_rev_id = 0U;
    uint32_t ppg_int_level = 0U;

    for (i = 0U; i < len && n < (sizeof(line) - 1U); i++)
    {
        char c = (char)buf[i];
        if (c == '\0')
        {
            break;
        }
        if (c == '\r' || c == '\n')
        {
            if (n == 0U)
            {
                continue;
            }
            break;
        }
        if ((unsigned char)c < 0x20U && c != '\t')
        {
            continue;
        }
        line[n++] = c;
    }
    line[n] = '\0';
    if (n == 0U)
    {
        return 0;
    }

    cursor = line;
    while ((cursor != NULL) && (token_count < 15U))
    {
        char *comma = strchr(cursor, ',');
        if (comma != NULL)
        {
            *comma = '\0';
            tokens[token_count++] = trim_spaces(cursor);
            cursor = comma + 1;
        }
        else
        {
            tokens[token_count++] = trim_spaces(cursor);
            cursor = NULL;
        }
    }

    if (token_count != 12U)
    {
        return 0;
    }

    if (tokens[0] != NULL && *tokens[0] != '\0')
    {
        if (!parse_u32_token(tokens[0], &sid))
        {
            return 0;
        }
    }
    if (!parse_u32_token(tokens[1], &seq))
    {
        return 0;
    }
    if (!parse_float_token(tokens[2], &y) ||
        !parse_float_token(tokens[3], &p) ||
        !parse_float_token(tokens[4], &r))
    {
        return 0;
    }
    if (!parse_i32_token(tokens[5], &heart_rate) ||
        !parse_i32_token(tokens[6], &hr_valid) ||
        !parse_i32_token(tokens[7], &spo2) ||
        !parse_i32_token(tokens[8], &spo2_valid))
    {
        return 0;
    }

    if (!parse_u32_token(tokens[9], &ppg_fill) ||
        !parse_u32_token(tokens[10], &ppg_calc_count))
    {
        return 0;
    }

    if (!parse_u32_token(tokens[11], &ppg_pending))
    {
        return 0;
    }

    if (sid > SENSOR_ID_FORE)
    {
        sid = default_sensor_id;
    }

    out->sensor_id = (uint8_t)sid;
    out->seq = seq;
    out->yaw = y;
    out->pitch = p;
    out->roll = r;
    out->heart_rate = heart_rate;
    out->spo2 = spo2;
    out->hr_valid = (int8_t)hr_valid;
    out->spo2_valid = (int8_t)spo2_valid;
    out->ppg_fill = ppg_fill;
    out->ppg_calc_count = ppg_calc_count;
    out->ppg_pending = ppg_pending;
    out->ppg_part_id = ppg_part_id;
    out->ppg_rev_id = ppg_rev_id;
    out->ppg_int_level = ppg_int_level;
    out->ts_us = get_ts_us();
    out->valid = 1U;
    return 1;
}

static void update_seq_stats(uint8_t sensor_id, uint32_t seq)
{
    uint32_t *last_seq = NULL;
    uint8_t *has_last = NULL;
    volatile uint32_t *lost = NULL;
    uint32_t *disorder = NULL;

    if (sensor_id == SENSOR_ID_UPPER)
    {
        last_seq = &last_seq_u;
        has_last = &has_last_seq_u;
        lost = &g_lost_u;
        disorder = &disorder_u;
    }
    else
    {
        last_seq = &last_seq_f;
        has_last = &has_last_seq_f;
        lost = &g_lost_f;
        disorder = &disorder_f;
    }

    if (!(*has_last))
    {
        *last_seq = seq;
        *has_last = 1U;
        return;
    }

    if (seq > (*last_seq + 1U))
    {
        *lost += (seq - *last_seq - 1U);
    }
    else if (seq <= *last_seq)
    {
        (*disorder)++;
    }

    *last_seq = seq;
}

static void try_emit_fused(void)
{
    uint64_t diff = 0ULL;

    if (!upper_frame.valid || !fore_frame.valid)
    {
        return;
    }

    diff = abs_diff_u64(upper_frame.ts_us, fore_frame.ts_us);

    if (diff <= ALIGN_THRESHOLD_US)
    {
        g_fused_ts_us = upper_frame.ts_us;
        g_fused_upper_yaw = upper_frame.yaw;
        g_fused_upper_pitch = upper_frame.pitch;
        g_fused_upper_roll = upper_frame.roll;
        g_fused_fore_yaw = fore_frame.yaw;
        g_fused_fore_pitch = fore_frame.pitch;
        g_fused_fore_roll = fore_frame.roll;
        g_fused_seq_u = upper_frame.seq;
        g_fused_seq_f = fore_frame.seq;
        g_fused_lost_u = g_lost_u;
        g_fused_lost_f = g_lost_f;
        g_fused_upper_heart_rate = upper_frame.heart_rate;
        g_fused_upper_spo2 = upper_frame.spo2;
        g_fused_upper_hr_valid = upper_frame.hr_valid;
        g_fused_upper_spo2_valid = upper_frame.spo2_valid;
        g_fused_upper_ppg_fill = upper_frame.ppg_fill;
        g_fused_upper_ppg_calc_count = upper_frame.ppg_calc_count;
        g_fused_upper_ppg_pending = upper_frame.ppg_pending;
        g_fused_upper_ppg_part_id = upper_frame.ppg_part_id;
        g_fused_upper_ppg_rev_id = upper_frame.ppg_rev_id;
        g_fused_upper_ppg_int_level = upper_frame.ppg_int_level;
        g_fused_fore_heart_rate = fore_frame.heart_rate;
        g_fused_fore_spo2 = fore_frame.spo2;
        g_fused_fore_hr_valid = fore_frame.hr_valid;
        g_fused_fore_spo2_valid = fore_frame.spo2_valid;
        g_fused_fore_ppg_fill = fore_frame.ppg_fill;
        g_fused_fore_ppg_calc_count = fore_frame.ppg_calc_count;
        g_fused_fore_ppg_pending = fore_frame.ppg_pending;
        g_fused_fore_ppg_part_id = fore_frame.ppg_part_id;
        g_fused_fore_ppg_rev_id = fore_frame.ppg_rev_id;
        g_fused_fore_ppg_int_level = fore_frame.ppg_int_level;
        g_fused_row_ready = 1U;

        /* Consume the frames so they do not get paired twice */
        /* Also prevents fail count increment when next frame overwrites */
        upper_frame.valid = 0U;
        fore_frame.valid = 0U;
    }
}

static void process_pose_packet(const uint8_t *buf, uint16_t len, uint8_t default_sensor_id)
{
    pose_frame_t frame = {0};

    if (!parse_pose_frame(buf, len, default_sensor_id, &frame))
    {
        return;
    }

    update_seq_stats(frame.sensor_id, frame.seq);

    if (frame.sensor_id == SENSOR_ID_UPPER)
    {
        if (upper_frame.valid)
        {
            g_align_fail_count++;
        }
        upper_frame = frame;
        yaw = frame.yaw;
        pitch = frame.pitch;
        roll = frame.roll;
    }
    else
    {
        if (fore_frame.valid)
        {
            g_align_fail_count++;
        }
        fore_frame = frame;
        yaw2 = frame.yaw;
        pitch2 = frame.pitch;
        roll2 = frame.roll;
    }

    try_emit_fused();
}

int extract_ypr(const uint8_t *buf, int len,
                float *yaw, float *pitch, float *roll)
{
const char *p   = (const char *)buf;
    const char *end = p + len;

    int found = 0;

    /* �ֲ� lambda����ȡһ������ */
    #define PARSE_KEY(key, dst) do {                                    \
        const char *k = (const char *)buf;                              \
            k = strstr(p, key);                                          \
        if (k) {                                                       \
            k += strlen(key);                                          \
            char tmp[16] = {0};                                        \
            int  n = 0;                                                \
            if (k < end && *k == '-') tmp[n++] = *k++;                 \
            /* �����ֺ�С���� */                                       \
            while (k < end && (isdigit((unsigned char)*k) || *k == '.') && n < 15) \
                tmp[n++] = *k++;                                       \
            if (n > 0) {                                               \
                tmp[n] = '\0';                                         \
                *(dst) = strtof(tmp, NULL);                            \
                ++found;                                               \
            }                                                          \
        }                                                              \
    } while (0)

    /* ˳���޹صؽ��������ֶ� */
    PARSE_KEY("yaw:",   yaw);
    PARSE_KEY("pitch:", pitch);
    PARSE_KEY("roll:",  roll);

    #undef PARSE_KEY
    return found;
}

static void reset_pose_pipeline(void)
{
    __disable_irq();
    upper_frame = (pose_frame_t){0};
    fore_frame = (pose_frame_t){0};

    has_last_seq_u = 0U;
    has_last_seq_f = 0U;
    last_seq_u = 0U;
    last_seq_f = 0U;
    g_lost_u = 0U;
    g_lost_f = 0U;
    disorder_u = 0U;
    disorder_f = 0U;
    g_align_fail_count = 0U;

    g_fused_row_ready = 0U;
    g_fused_ts_us = 0ULL;
    g_fused_upper_yaw = 0.0f;
    g_fused_upper_pitch = 0.0f;
    g_fused_upper_roll = 0.0f;
    g_fused_fore_yaw = 0.0f;
    g_fused_fore_pitch = 0.0f;
    g_fused_fore_roll = 0.0f;
    g_fused_seq_u = 0U;
    g_fused_seq_f = 0U;
    g_fused_lost_u = 0U;
    g_fused_lost_f = 0U;
    g_fused_upper_heart_rate = 0;
    g_fused_upper_spo2 = 0;
    g_fused_upper_hr_valid = 0;
    g_fused_upper_spo2_valid = 0;
    g_fused_upper_ppg_fill = 0U;
    g_fused_upper_ppg_calc_count = 0U;
    g_fused_upper_ppg_pending = 0U;
    g_fused_upper_ppg_part_id = 0U;
    g_fused_upper_ppg_rev_id = 0U;
    g_fused_upper_ppg_int_level = 0U;
    g_fused_fore_heart_rate = 0;
    g_fused_fore_spo2 = 0;
    g_fused_fore_hr_valid = 0;
    g_fused_fore_spo2_valid = 0;
    g_fused_fore_ppg_fill = 0U;
    g_fused_fore_ppg_calc_count = 0U;
    g_fused_fore_ppg_pending = 0U;
    g_fused_fore_ppg_part_id = 0U;
    g_fused_fore_ppg_rev_id = 0U;
    g_fused_fore_ppg_int_level = 0U;

    dwt_ts_inited = 0U;
    dwt_last_cyccnt = 0U;
    dwt_cycle_high = 0U;

    Calibrate_count = 0U;
    __enable_irq();

    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    memset(g_rx_buffer2, 0, sizeof(g_rx_buffer2));
}

static void motion_uart4_handle_command(const uint8_t *buf, uint16_t len)
{
    char line[64] = {0};
    uint16_t i = 0U;
    uint16_t n = 0U;
    char *command = NULL;

    if ((buf == NULL) || (len == 0U))
    {
        return;
    }

    for (i = 0U; i < len && n < (sizeof(line) - 1U); i++)
    {
        char c = (char)buf[i];
        if (c == '\0')
        {
            return;
        }
        if (c == '\r' || c == '\n')
        {
            if (n == 0U)
            {
                continue;
            }
            break;
        }
        line[n++] = c;
    }
    line[n] = '\0';

    command = trim_spaces(line);
    if (*command == '\0')
    {
        return;
    }

    if (strcmp(command, "start") == 0)
    {
        if (g_motion_output_mode == MOTION_OUTPUT_MODE_MODEL_WINDOW_TEST)
        {
            g_motion_single_armed = 0U;
            MotionWindowTest_RequestRun();
            return;
        }

        if (g_motion_ai_restart_req != 0U)
        {
            return;
        }

        if (g_motion_output_mode == MOTION_OUTPUT_MODE_SINGLE_ONCE)
        {
            g_motion_single_armed = 1U;
        }
        else
        {
            g_motion_single_armed = 0U;
        }

        reset_pose_pipeline();
        g_motion_ai_restart_req = 1U;
    }
}

/* USER CODE END 1 */
