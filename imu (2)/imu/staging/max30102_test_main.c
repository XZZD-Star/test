/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint16_t fill_count;
  uint8_t ready;
  uint8_t calc_req;
  uint8_t started;
  uint8_t last_pending;
  uint32_t total_samples;
  uint32_t calc_count;
  uint32_t last_red;
  uint32_t last_ir;
  uint32_t window_red_min;
  uint32_t window_red_max;
  uint32_t window_ir_min;
  uint32_t window_ir_max;
  int32_t heart_rate;
  int32_t spo2;
  int8_t hr_valid;
  int8_t spo2_valid;
} ppg_debug_state_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX30102_I2C_ADDR          (0x57U << 1)
#define N_IR_BUFFER_LENGTH         500
#define PPG_WINDOW_STEP            100
#define PPG_SAMPLE_PERIOD_MS       5U
#define PPG_PRINT_PERIOD_MS        500U

#define REG_INTR_STATUS_1          0x00
#define REG_INTR_STATUS_2          0x01
#define REG_INTR_ENABLE_1          0x02
#define REG_INTR_ENABLE_2          0x03
#define REG_FIFO_WR_PTR            0x04
#define REG_OVF_COUNTER            0x05
#define REG_FIFO_RD_PTR            0x06
#define REG_FIFO_DATA              0x07
#define REG_FIFO_CONFIG            0x08
#define REG_MODE_CONFIG            0x09
#define REG_SPO2_CONFIG            0x0A
#define REG_LED1_PA                0x0C
#define REG_LED2_PA                0x0D
#define REG_PILOT_PA               0x10
#define REG_PART_ID                0xFF
#define REG_REV_ID                 0xFE

#define MAX30102_INT()             HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9)

#define true 1
#define false 0
#define FS 100
#define BUFFER_SIZE (FS * 5)
#define HR_FIFO_SIZE 7
#define MA4_SIZE 4
#define HAMMING_SIZE 5
#define min(x,y) ((x) < (y) ? (x) : (y))
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint32_t g_red_buffer[N_IR_BUFFER_LENGTH];
static uint32_t g_ir_buffer[N_IR_BUFFER_LENGTH];
static uint16_t g_fill = 0;
static uint8_t g_ppg_ready = 0;
static uint8_t g_ppg_calc_req = 0;
static uint8_t g_ppg_started = 0;
static uint8_t g_last_pending = 0;
static uint32_t g_total_samples = 0;
static uint32_t g_calc_count = 0;
static uint32_t g_last_red = 0;
static uint32_t g_last_ir = 0;
static uint32_t g_window_red_min = 0;
static uint32_t g_window_red_max = 0;
static uint32_t g_window_ir_min = 0;
static uint32_t g_window_ir_max = 0;
static int32_t g_last_hr = -999;
static int32_t g_last_spo2 = -999;
static int8_t g_last_hr_valid = 0;
static int8_t g_last_spo2_valid = 0;
static uint8_t g_part_id = 0U;
static uint8_t g_rev_id = 0U;

static const uint16_t auw_hamm[31] = {41, 276, 512, 276, 41};
static const uint8_t uch_spo2_table[184] = {
  95,95,95,96,96,96,97,97,97,97,97,98,98,98,98,98,99,99,99,99,
  99,99,99,99,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,99,99,99,99,99,99,99,99,98,98,98,98,98,98,97,97,
  97,97,96,96,96,96,95,95,95,94,94,94,93,93,93,92,92,92,91,91,
  90,90,89,89,89,88,88,87,87,86,86,85,85,84,84,83,82,82,81,81,
  80,80,79,78,78,77,76,76,75,74,74,73,72,72,71,70,69,69,68,67,
  66,66,65,64,63,62,62,61,60,59,58,57,56,56,55,54,53,52,51,50,
  49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,31,30,29,
  28,27,26,25,23,22,21,20,19,17,16,15,14,12,11,10,9,7,6,5,
  3,2,1
};
static int32_t an_dx[BUFFER_SIZE - MA4_SIZE];
static int32_t an_x[BUFFER_SIZE];
static int32_t an_y[BUFFER_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void ppg_reset_state(void);
static HAL_StatusTypeDef max30102_write_reg(uint8_t reg, uint8_t value);
static HAL_StatusTypeDef max30102_read_reg(uint8_t reg, uint8_t *value);
static HAL_StatusTypeDef max30102_read_bytes(uint8_t reg, uint8_t *data, uint16_t len);
static void max30102_clear_interrupt_status(void);
static uint8_t max30102_fifo_pending(void);
static HAL_StatusTypeDef max30102_read_fifo_sample(uint32_t *red, uint32_t *ir);
static HAL_StatusTypeDef max30102_hw_init(void);
static void ppg_update_window_stats(void);
static void ppg_sample_task(void);
static void ppg_calc_task(void);
static void ppg_print_status(void);
static void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);
static void maxim_find_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num);
static void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height);
static void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance);
static void maxim_sort_ascend(int32_t *pn_x, int32_t n_size);
static void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if (__ARMCC_VERSION >= 6010050)
__asm(".global __use_no_semihosting\n\t");
__asm(".global __ARM_use_no_argv \n\t");
#else
#pragma import(__use_no_semihosting)
struct __FILE
{
  int handle;
};
#endif

FILE __stdout;

int _ttywrch(int ch)
{
  return ch;
}

void _sys_exit(int x)
{
  (void)x;
}

char *_sys_command_string(char *cmd, int len)
{
  (void)cmd;
  (void)len;
  return NULL;
}

int fputc(int ch, FILE *f)
{
  uint8_t c = (uint8_t)ch;
  (void)f;
  HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
  return ch;
}

static void ppg_reset_state(void)
{
  uint16_t i;

  g_fill = 0U;
  g_ppg_ready = 0U;
  g_ppg_calc_req = 0U;
  g_ppg_started = 0U;
  g_last_pending = 0U;
  g_total_samples = 0U;
  g_calc_count = 0U;
  g_last_red = 0U;
  g_last_ir = 0U;
  g_window_red_min = 0U;
  g_window_red_max = 0U;
  g_window_ir_min = 0U;
  g_window_ir_max = 0U;
  g_last_hr = -999;
  g_last_spo2 = -999;
  g_last_hr_valid = 0;
  g_last_spo2_valid = 0;

  for (i = 0U; i < N_IR_BUFFER_LENGTH; i++)
  {
    g_red_buffer[i] = 0U;
    g_ir_buffer[i] = 0U;
  }
}

static HAL_StatusTypeDef max30102_write_reg(uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c1, MAX30102_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1U, 100U);
}

static HAL_StatusTypeDef max30102_read_reg(uint8_t reg, uint8_t *value)
{
  return HAL_I2C_Mem_Read(&hi2c1, MAX30102_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, value, 1U, 100U);
}

static HAL_StatusTypeDef max30102_read_bytes(uint8_t reg, uint8_t *data, uint16_t len)
{
  return HAL_I2C_Mem_Read(&hi2c1, MAX30102_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, data, len, 100U);
}

static void max30102_clear_interrupt_status(void)
{
  uint8_t dummy = 0U;
  (void)max30102_read_reg(REG_INTR_STATUS_1, &dummy);
  (void)max30102_read_reg(REG_INTR_STATUS_2, &dummy);
}

static uint8_t max30102_fifo_pending(void)
{
  uint8_t wr = 0U;
  uint8_t rd = 0U;

  if (max30102_read_reg(REG_FIFO_WR_PTR, &wr) != HAL_OK)
  {
    return 0U;
  }

  if (max30102_read_reg(REG_FIFO_RD_PTR, &rd) != HAL_OK)
  {
    return 0U;
  }

  return (uint8_t)((wr - rd) & 0x1FU);
}

static HAL_StatusTypeDef max30102_read_fifo_sample(uint32_t *red, uint32_t *ir)
{
  uint8_t temp[6] = {0};
  HAL_StatusTypeDef status = max30102_read_bytes(REG_FIFO_DATA, temp, 6U);

  if (status != HAL_OK)
  {
    return status;
  }

  *red = ((uint32_t)(temp[0] & 0x03U) << 16) | ((uint32_t)temp[1] << 8) | temp[2];
  *ir = ((uint32_t)(temp[3] & 0x03U) << 16) | ((uint32_t)temp[4] << 8) | temp[5];
  return HAL_OK;
}

static HAL_StatusTypeDef max30102_hw_init(void)
{
  HAL_StatusTypeDef status = HAL_OK;

  HAL_Delay(20U);
  status = max30102_write_reg(REG_MODE_CONFIG, 0x40U);
  HAL_Delay(10U);
  if (status != HAL_OK)
  {
    return status;
  }

  (void)max30102_read_reg(REG_PART_ID, &g_part_id);
  (void)max30102_read_reg(REG_REV_ID, &g_rev_id);

  status = max30102_write_reg(REG_INTR_ENABLE_1, 0xC0U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_INTR_ENABLE_2, 0x00U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_FIFO_WR_PTR, 0x00U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_OVF_COUNTER, 0x00U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_FIFO_RD_PTR, 0x00U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_FIFO_CONFIG, 0x0FU);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_MODE_CONFIG, 0x03U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_SPO2_CONFIG, 0x27U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_LED1_PA, 0x24U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_LED2_PA, 0x24U);
  if (status != HAL_OK) return status;
  status = max30102_write_reg(REG_PILOT_PA, 0x7FU);
  if (status != HAL_OK) return status;

  max30102_clear_interrupt_status();
  ppg_reset_state();
  g_ppg_started = 1U;
  return HAL_OK;
}

static void ppg_update_window_stats(void)
{
  uint16_t i;
  uint32_t red_min = g_red_buffer[0];
  uint32_t red_max = g_red_buffer[0];
  uint32_t ir_min = g_ir_buffer[0];
  uint32_t ir_max = g_ir_buffer[0];

  for (i = 1U; i < N_IR_BUFFER_LENGTH; i++)
  {
    if (g_red_buffer[i] < red_min) red_min = g_red_buffer[i];
    if (g_red_buffer[i] > red_max) red_max = g_red_buffer[i];
    if (g_ir_buffer[i] < ir_min) ir_min = g_ir_buffer[i];
    if (g_ir_buffer[i] > ir_max) ir_max = g_ir_buffer[i];
  }

  g_window_red_min = red_min;
  g_window_red_max = red_max;
  g_window_ir_min = ir_min;
  g_window_ir_max = ir_max;
}

static void ppg_sample_task(void)
{
  uint8_t pending;

  if (!g_ppg_started || g_ppg_calc_req)
  {
    return;
  }

  pending = max30102_fifo_pending();
  g_last_pending = pending;
  if (pending == 0U)
  {
    return;
  }

  if (MAX30102_INT() == GPIO_PIN_RESET)
  {
    max30102_clear_interrupt_status();
  }

  while ((pending > 0U) && (g_fill < N_IR_BUFFER_LENGTH))
  {
    uint32_t red = 0U;
    uint32_t ir = 0U;

    if (max30102_read_fifo_sample(&red, &ir) != HAL_OK)
    {
      break;
    }

    g_red_buffer[g_fill] = red;
    g_ir_buffer[g_fill] = ir;
    g_last_red = red;
    g_last_ir = ir;
    g_fill++;
    g_total_samples++;
    pending--;
  }

  if (g_fill >= N_IR_BUFFER_LENGTH)
  {
    g_ppg_calc_req = 1U;
  }
}

static void ppg_calc_task(void)
{
  uint16_t i;

  if (!g_ppg_calc_req)
  {
    return;
  }

  ppg_update_window_stats();
  maxim_heart_rate_and_oxygen_saturation(g_ir_buffer,
                                         N_IR_BUFFER_LENGTH,
                                         g_red_buffer,
                                         &g_last_spo2,
                                         &g_last_spo2_valid,
                                         &g_last_hr,
                                         &g_last_hr_valid);

  g_ppg_ready = 1U;
  g_calc_count++;

  for (i = PPG_WINDOW_STEP; i < N_IR_BUFFER_LENGTH; i++)
  {
    g_red_buffer[i - PPG_WINDOW_STEP] = g_red_buffer[i];
    g_ir_buffer[i - PPG_WINDOW_STEP] = g_ir_buffer[i];
  }

  g_fill = N_IR_BUFFER_LENGTH - PPG_WINDOW_STEP;
  g_ppg_calc_req = 0U;
}

static void ppg_print_status(void)
{
  printf("PPGDBG,part=0x%02X,rev=0x%02X,int=%u,fill=%u,ready=%u,calc_req=%u,started=%u,pending=%u,total=%lu,calc=%lu,last_red=%lu,last_ir=%lu,rmin=%lu,rmax=%lu,imin=%lu,imax=%lu,hr=%ld,hrv=%d,spo2=%ld,sv=%d\r\n",
         (unsigned int)g_part_id,
         (unsigned int)g_rev_id,
         (unsigned int)(MAX30102_INT() == GPIO_PIN_RESET ? 0U : 1U),
         (unsigned int)g_fill,
         (unsigned int)g_ppg_ready,
         (unsigned int)g_ppg_calc_req,
         (unsigned int)g_ppg_started,
         (unsigned int)g_last_pending,
         (unsigned long)g_total_samples,
         (unsigned long)g_calc_count,
         (unsigned long)g_last_red,
         (unsigned long)g_last_ir,
         (unsigned long)g_window_red_min,
         (unsigned long)g_window_red_max,
         (unsigned long)g_window_ir_min,
         (unsigned long)g_window_ir_max,
         (long)g_last_hr,
         (int)g_last_hr_valid,
         (long)g_last_spo2,
         (int)g_last_spo2_valid);
}

static void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{
  uint32_t un_ir_mean, un_only_once;
  int32_t k, n_i_ratio_count;
  int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
  int32_t n_th1, n_npks, n_c_min;
  int32_t an_ir_valley_locs[15];
  int32_t an_exact_ir_valley_locs[15];
  int32_t an_dx_peak_locs[15];
  int32_t n_peak_interval_sum;
  int32_t n_y_ac, n_x_ac;
  int32_t n_spo2_calc;
  int32_t n_y_dc_max, n_x_dc_max;
  int32_t n_y_dc_max_idx = 0;
  int32_t n_x_dc_max_idx = 0;
  int32_t an_ratio[5], n_ratio_average;
  int32_t n_nume, n_denom;

  un_ir_mean = 0U;
  for (k = 0; k < n_ir_buffer_length; k++)
  {
    un_ir_mean += pun_ir_buffer[k];
  }
  un_ir_mean = un_ir_mean / (uint32_t)n_ir_buffer_length;

  for (k = 0; k < n_ir_buffer_length; k++)
  {
    an_x[k] = (int32_t)pun_ir_buffer[k] - (int32_t)un_ir_mean;
  }

  for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++)
  {
    n_denom = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]);
    an_x[k] = n_denom / 4;
  }

  for (k = 0; k < BUFFER_SIZE - MA4_SIZE - 1; k++)
  {
    an_dx[k] = an_x[k + 1] - an_x[k];
  }

  for (k = 0; k < BUFFER_SIZE - MA4_SIZE - 2; k++)
  {
    an_dx[k] = (an_dx[k] + an_dx[k + 1]) / 2;
  }

  for (i = 0; i < BUFFER_SIZE - HAMMING_SIZE - MA4_SIZE - 2; i++)
  {
    s = 0;
    for (k = i; k < i + HAMMING_SIZE; k++)
    {
      s -= an_dx[k] * auw_hamm[k - i];
    }
    an_dx[i] = s / 1146;
  }

  n_th1 = 0;
  for (k = 0; k < BUFFER_SIZE - HAMMING_SIZE; k++)
  {
    n_th1 += (an_dx[k] > 0) ? an_dx[k] : (0 - an_dx[k]);
  }
  n_th1 = n_th1 / (BUFFER_SIZE - HAMMING_SIZE);

  maxim_find_peaks(an_dx_peak_locs, &n_npks, an_dx, BUFFER_SIZE - HAMMING_SIZE, n_th1, 8, 5);

  n_peak_interval_sum = 0;
  if (n_npks >= 2)
  {
    for (k = 1; k < n_npks; k++)
    {
      n_peak_interval_sum += (an_dx_peak_locs[k] - an_dx_peak_locs[k - 1]);
    }
    n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
    *pn_heart_rate = (int32_t)(6000 / n_peak_interval_sum);
    *pch_hr_valid = 1;
  }
  else
  {
    *pn_heart_rate = -999;
    *pch_hr_valid = 0;
  }

  for (k = 0; k < n_npks; k++)
  {
    an_ir_valley_locs[k] = an_dx_peak_locs[k] + HAMMING_SIZE / 2;
  }

  for (k = 0; k < n_ir_buffer_length; k++)
  {
    an_x[k] = (int32_t)pun_ir_buffer[k];
    an_y[k] = (int32_t)pun_red_buffer[k];
  }

  n_exact_ir_valley_locs_count = 0;
  for (k = 0; k < n_npks; k++)
  {
    un_only_once = 1U;
    m = an_ir_valley_locs[k];
    n_c_min = 16777216;
    if (m + 5 < BUFFER_SIZE - HAMMING_SIZE && m - 5 > 0)
    {
      for (i = m - 5; i < m + 5; i++)
      {
        if (an_x[i] < n_c_min)
        {
          if (un_only_once > 0U)
          {
            un_only_once = 0U;
          }
          n_c_min = an_x[i];
          an_exact_ir_valley_locs[k] = i;
        }
      }
      if (un_only_once == 0U)
      {
        n_exact_ir_valley_locs_count++;
      }
    }
  }

  if (n_exact_ir_valley_locs_count < 2)
  {
    *pn_spo2 = -999;
    *pch_spo2_valid = 0;
    return;
  }

  for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++)
  {
    an_x[k] = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]) / 4;
    an_y[k] = (an_y[k] + an_y[k + 1] + an_y[k + 2] + an_y[k + 3]) / 4;
  }

  n_ratio_average = 0;
  n_i_ratio_count = 0;
  for (k = 0; k < 5; k++)
  {
    an_ratio[k] = 0;
  }

  for (k = 0; k < n_exact_ir_valley_locs_count; k++)
  {
    if (an_exact_ir_valley_locs[k] > BUFFER_SIZE)
    {
      *pn_spo2 = -999;
      *pch_spo2_valid = 0;
      return;
    }
  }

  for (k = 0; k < n_exact_ir_valley_locs_count - 1; k++)
  {
    n_y_dc_max = -16777216;
    n_x_dc_max = -16777216;
    if (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k] > 10)
    {
      for (i = an_exact_ir_valley_locs[k]; i < an_exact_ir_valley_locs[k + 1]; i++)
      {
        if (an_x[i] > n_x_dc_max)
        {
          n_x_dc_max = an_x[i];
          n_x_dc_max_idx = i;
        }
        if (an_y[i] > n_y_dc_max)
        {
          n_y_dc_max = an_y[i];
          n_y_dc_max_idx = i;
        }
      }
      n_y_ac = (an_y[an_exact_ir_valley_locs[k + 1]] - an_y[an_exact_ir_valley_locs[k]]) * (n_y_dc_max_idx - an_exact_ir_valley_locs[k]);
      n_y_ac = an_y[an_exact_ir_valley_locs[k]] + n_y_ac / (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
      n_y_ac = an_y[n_y_dc_max_idx] - n_y_ac;
      n_x_ac = (an_x[an_exact_ir_valley_locs[k + 1]] - an_x[an_exact_ir_valley_locs[k]]) * (n_x_dc_max_idx - an_exact_ir_valley_locs[k]);
      n_x_ac = an_x[an_exact_ir_valley_locs[k]] + n_x_ac / (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
      n_x_ac = an_x[n_y_dc_max_idx] - n_x_ac;
      n_nume = (n_y_ac * n_x_dc_max) >> 7;
      n_denom = (n_x_ac * n_y_dc_max) >> 7;
      if ((n_denom > 0) && (n_i_ratio_count < 5) && (n_nume != 0))
      {
        an_ratio[n_i_ratio_count] = (n_nume * 20) / n_denom;
        n_i_ratio_count++;
      }
    }
  }

  maxim_sort_ascend(an_ratio, n_i_ratio_count);
  n_middle_idx = n_i_ratio_count / 2;
  if (n_middle_idx > 1)
  {
    n_ratio_average = (an_ratio[n_middle_idx - 1] + an_ratio[n_middle_idx]) / 2;
  }
  else
  {
    n_ratio_average = an_ratio[n_middle_idx];
  }

  if (n_ratio_average > 2 && n_ratio_average < 184)
  {
    n_spo2_calc = uch_spo2_table[n_ratio_average];
    *pn_spo2 = n_spo2_calc;
    *pch_spo2_valid = 1;
  }
  else
  {
    *pn_spo2 = -999;
    *pch_spo2_valid = 0;
  }
}

static void maxim_find_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num)
{
  maxim_peaks_above_min_height(pn_locs, pn_npks, pn_x, n_size, n_min_height);
  maxim_remove_close_peaks(pn_locs, pn_npks, pn_x, n_min_distance);
  *pn_npks = min(*pn_npks, n_max_num);
}

static void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height)
{
  int32_t i = 1;
  int32_t n_width;

  *pn_npks = 0;
  while (i < n_size - 1)
  {
    if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i - 1])
    {
      n_width = 1;
      while (i + n_width < n_size && pn_x[i] == pn_x[i + n_width])
      {
        n_width++;
      }
      if (pn_x[i] > pn_x[i + n_width] && (*pn_npks) < 15)
      {
        pn_locs[(*pn_npks)++] = i;
        i += n_width + 1;
      }
      else
      {
        i += n_width;
      }
    }
    else
    {
      i++;
    }
  }
}

static void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance)
{
  int32_t i;
  int32_t j;
  int32_t n_old_npks;
  int32_t n_dist;

  maxim_sort_indices_descend(pn_x, pn_locs, *pn_npks);
  for (i = -1; i < *pn_npks; i++)
  {
    n_old_npks = *pn_npks;
    *pn_npks = i + 1;
    for (j = i + 1; j < n_old_npks; j++)
    {
      n_dist = pn_locs[j] - (i == -1 ? -1 : pn_locs[i]);
      if (n_dist > n_min_distance || n_dist < -n_min_distance)
      {
        pn_locs[(*pn_npks)++] = pn_locs[j];
      }
    }
  }

  maxim_sort_ascend(pn_locs, *pn_npks);
}

static void maxim_sort_ascend(int32_t *pn_x, int32_t n_size)
{
  int32_t i;
  int32_t j;
  int32_t n_temp;

  for (i = 1; i < n_size; i++)
  {
    n_temp = pn_x[i];
    for (j = i; j > 0 && n_temp < pn_x[j - 1]; j--)
    {
      pn_x[j] = pn_x[j - 1];
    }
    pn_x[j] = n_temp;
  }
}

static void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)
{
  int32_t i;
  int32_t j;
  int32_t n_temp;

  for (i = 1; i < n_size; i++)
  {
    n_temp = pn_indx[i];
    for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j - 1]]; j--)
    {
      pn_indx[j] = pn_indx[j - 1];
    }
    pn_indx[j] = n_temp;
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  uint32_t last_sample_ms = 0U;
  uint32_t last_print_ms = 0U;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();

  HAL_Delay(50U);
  printf("\r\n=== MAX30102 PPG TEST ===\r\n");
  if (max30102_hw_init() != HAL_OK)
  {
    printf("PPGERR,init_failed\r\n");
    Error_Handler();
  }

  printf("PPGINFO,part=0x%02X,rev=0x%02X,int=%u\r\n",
         (unsigned int)g_part_id,
         (unsigned int)g_rev_id,
         (unsigned int)(MAX30102_INT() == GPIO_PIN_RESET ? 0U : 1U));

  last_sample_ms = HAL_GetTick();
  last_print_ms = HAL_GetTick();

  while (1)
  {
    uint32_t now = HAL_GetTick();

    if ((now - last_sample_ms) >= PPG_SAMPLE_PERIOD_MS)
    {
      last_sample_ms += PPG_SAMPLE_PERIOD_MS;
      ppg_sample_task();
    }

    ppg_calc_task();

    if ((now - last_print_ms) >= PPG_PRINT_PERIOD_MS)
    {
      last_print_ms += PPG_PRINT_PERIOD_MS;
      ppg_print_status();
    }
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */

