#include "ESP8266.h"

#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "debug_uart7.h"
#include "usart.h"

#define ESP8266_RX_BUFFER_SIZE         1024U
#define ESP8266_CMD_BUFFER_SIZE         160U
#define ESP8266_POLL_DELAY_MS            10U
#define ESP8266_RX_IDLE_MS               20U
#define ESP8266_AT_TIMEOUT_MS          3000U
#define ESP8266_WIFI_TIMEOUT_MS       40000U
#define ESP8266_TCP_TIMEOUT_MS        10000U
#define ESP8266_CIPSEND_TIMEOUT_MS     3000U
#define ESP8266_IPD_SCAN_SLICE_MS        40U
#define ESP8266_IPD_RETRY_DELAY_MS        5U
#define ESP8266_IPD_STALL_MS            200U
#define ESP8266_IPD_STATE_NONE           0U
#define ESP8266_IPD_STATE_INCOMPLETE     1U
#define ESP8266_IPD_STATE_COMPLETE       2U

static volatile uint16_t g_esp8266_rx_count = 0U;
static volatile uint32_t g_esp8266_rx_last_tick = 0U;
static volatile uint8_t g_esp8266_transport_error = 0U;
static volatile uint8_t g_esp8266_last_init_status = ESP8266_INIT_STATUS_OK;
static uint8_t g_esp8266_rx_buffer[ESP8266_RX_BUFFER_SIZE];

static uint8_t ESP8266_SendCmdInternal(const char *cmd,
                                       const char *expect,
                                       const char *alt_expect,
                                       uint32_t timeout_ms);
static void ESP8266_TryCloseSocket(void);
static uint8_t ESP8266_CheckIPDPayload(uint32_t *ipd_len,
                                       uint16_t *payload_offset,
                                       uint16_t *available_len,
                                       uint8_t **payload_ptr);
static uint8_t ESP8266_IsTxAckBuffer(void);
static void ESP8266_DebugPrintHexPrefix(const uint8_t *data,
                                        uint16_t len,
                                        uint16_t limit);

static uint32_t ESP8266_GetTickMs(void)
{
  if (osKernelGetState() == osKernelRunning)
  {
    return osKernelGetTickCount();
  }

  return HAL_GetTick();
}

static void ESP8266_SleepMs(uint32_t delay_ms)
{
  if ((delay_ms == 0U) || (osKernelGetState() != osKernelRunning))
  {
    return;
  }

  osDelay(delay_ms);
}

static void ESP8266_SendRaw(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart6, (uint8_t *)data, len, HAL_MAX_DELAY);
}

static uint16_t ESP8266_GetRxCountSnapshot(void)
{
  uint16_t count;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  count = g_esp8266_rx_count;
  if (primask == 0U)
  {
    __enable_irq();
  }

  return count;
}

static uint32_t ESP8266_GetRxLastTickSnapshot(void)
{
  uint32_t tick;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  tick = g_esp8266_rx_last_tick;
  if (primask == 0U)
  {
    __enable_irq();
  }

  return tick;
}

static void ESP8266_DebugPrintHexPrefix(const uint8_t *data,
                                        uint16_t len,
                                        uint16_t limit)
{
  char line[160];
  uint16_t i = 0U;
  int pos = 0;

  if ((data == NULL) || (len == 0U))
  {
    Debug_Printf("[TCP] RX HEX <empty>\r\n");
    return;
  }

  if ((limit == 0U) || (limit > len))
  {
    limit = len;
  }

  pos = snprintf(line, sizeof(line), "[TCP] RX HEX");
  for (i = 0U; (i < limit) && (pos > 0) && ((size_t)pos < (sizeof(line) - 4U)); i++)
  {
    pos += snprintf(&line[pos], sizeof(line) - (size_t)pos, " %02X", data[i]);
  }

  if ((len > limit) && (pos > 0) && ((size_t)pos < (sizeof(line) - 5U)))
  {
    (void)snprintf(&line[pos], sizeof(line) - (size_t)pos, " ...");
  }

  Debug_Printf("%s\r\n", line);
}

static uint8_t ESP8266_BufferContains(const uint8_t *buffer, uint16_t length, const char *needle)
{
  size_t needle_len = 0U;
  uint16_t i = 0U;

  if ((buffer == NULL) || (needle == NULL))
  {
    return 0U;
  }

  needle_len = strlen(needle);
  if ((needle_len == 0U) || (length < needle_len))
  {
    return 0U;
  }

  for (i = 0U; i <= (uint16_t)(length - needle_len); i++)
  {
    if (memcmp(&buffer[i], needle, needle_len) == 0)
    {
      return 1U;
    }
  }

  return 0U;
}

static uint8_t ESP8266_CurrentBufferContains(const char *needle)
{
  uint16_t count = ESP8266_GetRxCountSnapshot();
  return ESP8266_BufferContains(g_esp8266_rx_buffer, count, needle);
}

static uint8_t ESP8266_WaitForPattern(const char *expect, uint32_t timeout_ms)
{
  uint32_t start = ESP8266_GetTickMs();

  if ((expect == NULL) || (*expect == '\0'))
  {
    return 0U;
  }

  while ((ESP8266_GetTickMs() - start) < timeout_ms)
  {
    if (ESP8266_CurrentBufferContains(expect) != 0U)
    {
      return 1U;
    }

    if (ESP8266_CurrentBufferContains("ERROR") != 0U)
    {
      return 0U;
    }

    if (ESP8266_CurrentBufferContains("FAIL") != 0U)
    {
      return 0U;
    }

    if (ESP8266_HasTransportError() != 0U)
    {
      return 0U;
    }

    ESP8266_SleepMs(ESP8266_POLL_DELAY_MS);
  }

  return 0U;
}

static uint8_t ESP8266_IsTargetWifiConnected(void)
{
  if (ESP8266_SendCmdInternal("AT+CWJAP?", "OK", NULL, ESP8266_AT_TIMEOUT_MS) == 0U)
  {
    return 0U;
  }

  return ESP8266_CurrentBufferContains(WIFI_SSID);
}

static uint8_t ESP8266_WaitForEitherPattern(const char *expect,
                                            const char *alt_expect,
                                            uint32_t timeout_ms)
{
  uint32_t start = ESP8266_GetTickMs();

  if ((expect == NULL) || (*expect == '\0'))
  {
    return 0U;
  }

  while ((ESP8266_GetTickMs() - start) < timeout_ms)
  {
    if (ESP8266_CurrentBufferContains(expect) != 0U)
    {
      return 1U;
    }

    if ((alt_expect != NULL) && (*alt_expect != '\0') &&
        (ESP8266_CurrentBufferContains(alt_expect) != 0U))
    {
      return 1U;
    }

    if (ESP8266_CurrentBufferContains("ERROR") != 0U)
    {
      return 0U;
    }

    if (ESP8266_CurrentBufferContains("FAIL") != 0U)
    {
      return 0U;
    }

    if (ESP8266_HasTransportError() != 0U)
    {
      return 0U;
    }

    ESP8266_SleepMs(ESP8266_POLL_DELAY_MS);
  }

  return 0U;
}

static void ESP8266_TryCloseSocket(void)
{
  static const uint8_t close_cmd[] = "AT+CIPCLOSE\r\n";

  ESP8266_Clear();
  ESP8266_ClearTransportError();
  ESP8266_SendRaw(close_cmd, (uint16_t)(sizeof(close_cmd) - 1U));
  (void)ESP8266_WaitReceive(1000U);
  ESP8266_SleepMs(200U);
  ESP8266_Clear();
  ESP8266_ClearTransportError();
}

static uint8_t ESP8266_CheckIPDPayload(uint32_t *ipd_len,
                                       uint16_t *payload_offset,
                                       uint16_t *available_len,
                                       uint8_t **payload_ptr)
{
  uint16_t count = ESP8266_GetRxCountSnapshot();
  uint16_t i = 0U;

  if (ipd_len != NULL)
  {
    *ipd_len = 0U;
  }
  if (payload_offset != NULL)
  {
    *payload_offset = 0U;
  }
  if (available_len != NULL)
  {
    *available_len = 0U;
  }
  if (payload_ptr != NULL)
  {
    *payload_ptr = NULL;
  }

  for (i = 0U; (i + 5U) <= count; i++)
  {
    uint16_t j = 0U;
    uint32_t local_ipd_len = 0U;
    uint8_t has_len_digit = 0U;

    if (memcmp(&g_esp8266_rx_buffer[i], "+IPD,", 5U) != 0)
    {
      continue;
    }

    j = (uint16_t)(i + 5U);
    while ((j < count) && (g_esp8266_rx_buffer[j] >= '0') && (g_esp8266_rx_buffer[j] <= '9'))
    {
      has_len_digit = 1U;
      local_ipd_len = (local_ipd_len * 10U) + (uint32_t)(g_esp8266_rx_buffer[j] - '0');
      j++;
    }

    if (has_len_digit == 0U)
    {
      return ESP8266_IPD_STATE_INCOMPLETE;
    }

    while ((j < count) && (g_esp8266_rx_buffer[j] != ':'))
    {
      j++;
    }

    if (j >= count)
    {
      return ESP8266_IPD_STATE_INCOMPLETE;
    }

    j++;
    if (ipd_len != NULL)
    {
      *ipd_len = local_ipd_len;
    }
    if (payload_offset != NULL)
    {
      *payload_offset = j;
    }
    if (available_len != NULL)
    {
      *available_len = (uint16_t)((uint32_t)count - (uint32_t)j);
    }

    if (((uint32_t)count - (uint32_t)j) < local_ipd_len)
    {
      return ESP8266_IPD_STATE_INCOMPLETE;
    }

    if (payload_ptr != NULL)
    {
      *payload_ptr = &g_esp8266_rx_buffer[j];
    }

    return ESP8266_IPD_STATE_COMPLETE;
  }

  return ESP8266_IPD_STATE_NONE;
}

static uint8_t ESP8266_IsTxAckBuffer(void)
{
  uint16_t count = ESP8266_GetRxCountSnapshot();

  if (count == 0U)
  {
    return 0U;
  }

  if ((ESP8266_BufferContains(g_esp8266_rx_buffer, count, "SEND OK") != 0U) ||
      (ESP8266_BufferContains(g_esp8266_rx_buffer, count, "Recv ") != 0U) ||
      (ESP8266_BufferContains(g_esp8266_rx_buffer, count, "\r\n>\r\n") != 0U))
  {
    return 1U;
  }

  return 0U;
}

static uint8_t ESP8266_SendCmdInternal(const char *cmd,
                                       const char *expect,
                                       const char *alt_expect,
                                       uint32_t timeout_ms)
{
  size_t cmd_len = 0U;
  static const uint8_t crlf[] = "\r\n";

  if ((cmd == NULL) || (*cmd == '\0'))
  {
    return 0U;
  }

  ESP8266_Clear();
  cmd_len = strlen(cmd);
  ESP8266_SendRaw((const uint8_t *)cmd, (uint16_t)cmd_len);

  if ((cmd_len < 2U) || (cmd[cmd_len - 2U] != '\r') || (cmd[cmd_len - 1U] != '\n'))
  {
    ESP8266_SendRaw(crlf, (uint16_t)(sizeof(crlf) - 1U));
  }

  if (ESP8266_WaitForEitherPattern(expect, alt_expect, timeout_ms) != 0U)
  {
    return 1U;
  }

  return 0U;
}

void ESP8266_Clear(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  memset(g_esp8266_rx_buffer, 0, sizeof(g_esp8266_rx_buffer));
  g_esp8266_rx_count = 0U;
  g_esp8266_rx_last_tick = 0U;
  if (primask == 0U)
  {
    __enable_irq();
  }
}

uint8_t ESP8266_WaitReceive(uint32_t timeout_ms)
{
  uint32_t start = ESP8266_GetTickMs();

  while ((ESP8266_GetTickMs() - start) < timeout_ms)
  {
    uint16_t count = ESP8266_GetRxCountSnapshot();
    uint32_t last_tick = ESP8266_GetRxLastTickSnapshot();

    if ((count > 0U) && ((ESP8266_GetTickMs() - last_tick) >= ESP8266_RX_IDLE_MS))
    {
      return 1U;
    }

    if (ESP8266_HasTransportError() != 0U)
    {
      return 0U;
    }

    ESP8266_SleepMs(ESP8266_POLL_DELAY_MS);
  }

  return 0U;
}

uint8_t ESP8266_SendCmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
  return ESP8266_SendCmdInternal(cmd, expect, NULL, timeout_ms);
}

uint8_t ESP8266_SendData(const uint8_t *data, uint16_t len)
{
  char cmd[ESP8266_CMD_BUFFER_SIZE];

  if ((data == NULL) || (len == 0U))
  {
    return 0U;
  }

  (void)snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned int)len);
  if (ESP8266_SendCmd(cmd, ">", ESP8266_CIPSEND_TIMEOUT_MS) == 0U)
  {
    return 0U;
  }

  ESP8266_SendRaw(data, len);
  return 1U;
}

uint8_t *ESP8266_GetIPD(uint32_t timeout_ms)
{
  uint32_t start = ESP8266_GetTickMs();
  static uint16_t s_last_incomplete_count = 0U;
  static uint16_t s_last_incomplete_have = 0U;
  static uint32_t s_last_incomplete_need = 0U;

  while ((ESP8266_GetTickMs() - start) < timeout_ms)
  {
    uint32_t elapsed = ESP8266_GetTickMs() - start;
    uint32_t wait_window = timeout_ms - elapsed;
    uint16_t count = 0U;
    uint16_t payload_offset = 0U;
    uint16_t available_len = 0U;
    uint32_t ipd_len = 0U;
    uint8_t *payload_ptr = NULL;
    uint8_t ipd_state = ESP8266_IPD_STATE_NONE;

    if (wait_window > ESP8266_IPD_SCAN_SLICE_MS)
    {
      wait_window = ESP8266_IPD_SCAN_SLICE_MS;
    }

    if (ESP8266_WaitReceive(wait_window) != 0U)
    {
      count = ESP8266_GetRxCountSnapshot();
      ipd_state = ESP8266_CheckIPDPayload(&ipd_len,
                                          &payload_offset,
                                          &available_len,
                                          &payload_ptr);

      if ((ipd_state == ESP8266_IPD_STATE_COMPLETE) && (payload_ptr != NULL))
      {
        s_last_incomplete_count = 0U;
        s_last_incomplete_have = 0U;
        s_last_incomplete_need = 0U;
        Debug_Printf("[TCP] +IPD len=%lu offset=%u rx_count=%u\r\n",
                     (unsigned long)ipd_len,
                     (unsigned int)payload_offset,
                     (unsigned int)count);
        ESP8266_DebugDumpCurrentRx("IPD", 48U);
        return payload_ptr;
      }

      if (ipd_state == ESP8266_IPD_STATE_INCOMPLETE)
      {
        uint32_t idle_ms = ESP8266_GetTickMs() - ESP8266_GetRxLastTickSnapshot();

        if ((count != s_last_incomplete_count) ||
            (available_len != s_last_incomplete_have) ||
            (ipd_len != s_last_incomplete_need))
        {
          Debug_Printf("[TCP] +IPD incomplete need=%lu have=%u count=%u idle=%lu\r\n",
                       (unsigned long)ipd_len,
                       (unsigned int)available_len,
                       (unsigned int)count,
                       (unsigned long)idle_ms);
          ESP8266_DebugDumpCurrentRx("IPD_WAIT", 48U);
          s_last_incomplete_count = count;
          s_last_incomplete_have = available_len;
          s_last_incomplete_need = ipd_len;
        }

        if (idle_ms >= ESP8266_IPD_STALL_MS)
        {
          Debug_Printf("[TCP][ERR] +IPD stalled drop need=%lu have=%u idle=%lu\r\n",
                       (unsigned long)ipd_len,
                       (unsigned int)available_len,
                       (unsigned long)idle_ms);
          ESP8266_DebugDumpCurrentRx("IPD_STALL", 48U);
          ESP8266_Clear();
          s_last_incomplete_count = 0U;
          s_last_incomplete_have = 0U;
          s_last_incomplete_need = 0U;
        }

        ESP8266_SleepMs(ESP8266_IPD_RETRY_DELAY_MS);
        continue;
      }

      if (count > 0U)
      {
        s_last_incomplete_count = 0U;
        s_last_incomplete_have = 0U;
        s_last_incomplete_need = 0U;
        if (ESP8266_IsTxAckBuffer() == 0U)
        {
          Debug_Printf("[TCP] RX without +IPD, drop count=%u\r\n", (unsigned int)count);
          ESP8266_DebugDumpCurrentRx("NO_IPD", 48U);
        }
        ESP8266_Clear();
      }
    }

    if (ESP8266_HasTransportError() != 0U)
    {
      s_last_incomplete_count = 0U;
      s_last_incomplete_have = 0U;
      s_last_incomplete_need = 0U;
      return NULL;
    }

    ESP8266_SleepMs(ESP8266_IPD_RETRY_DELAY_MS);
  }

  s_last_incomplete_count = 0U;
  s_last_incomplete_have = 0U;
  s_last_incomplete_need = 0U;
  return NULL;
}

uint8_t ESP8266_Init(void)
{
  char cmd[ESP8266_CMD_BUFFER_SIZE];

  ESP8266_Clear();
  ESP8266_ClearTransportError();
  g_esp8266_last_init_status = ESP8266_INIT_STATUS_OK;

  if (ESP8266_SendCmd("AT", "OK", ESP8266_AT_TIMEOUT_MS) == 0U)
  {
    g_esp8266_last_init_status = ESP8266_INIT_STATUS_FAIL_AT;
    return 0U;
  }

  if (ESP8266_SendCmd("ATE0", "OK", ESP8266_AT_TIMEOUT_MS) == 0U)
  {
    g_esp8266_last_init_status = ESP8266_INIT_STATUS_FAIL_ATE0;
    return 0U;
  }

  if (ESP8266_SendCmd("AT+CWMODE=1", "OK", ESP8266_AT_TIMEOUT_MS) == 0U)
  {
    g_esp8266_last_init_status = ESP8266_INIT_STATUS_FAIL_CWMODE;
    return 0U;
  }

  if (ESP8266_SendCmd("AT+CWDHCP=1,1", "OK", ESP8266_AT_TIMEOUT_MS) == 0U)
  {
    g_esp8266_last_init_status = ESP8266_INIT_STATUS_FAIL_DHCP;
    return 0U;
  }

  if (ESP8266_IsTargetWifiConnected() == 0U)
  {
    (void)snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASSWORD);
    if (ESP8266_SendCmd(cmd, "GOT IP", ESP8266_WIFI_TIMEOUT_MS) == 0U)
    {
      g_esp8266_last_init_status = ESP8266_INIT_STATUS_FAIL_CWJAP;
      return 0U;
    }
  }

  ESP8266_TryCloseSocket();

  if (ESP8266_SendCmd("AT+CIPMUX=0", "OK", ESP8266_AT_TIMEOUT_MS) == 0U)
  {
    g_esp8266_last_init_status = ESP8266_INIT_STATUS_FAIL_CIPMUX;
    return 0U;
  }

  (void)snprintf(cmd,
                 sizeof(cmd),
                 "AT+CIPSTART=\"TCP\",\"%s\",%u",
                 ONENET_HOST,
                 (unsigned int)ONENET_PORT);
  if (ESP8266_SendCmdInternal(cmd, "CONNECT", "ALREADY CONNECTED", ESP8266_TCP_TIMEOUT_MS) == 0U)
  {
    g_esp8266_last_init_status = ESP8266_INIT_STATUS_FAIL_CIPSTART;
    return 0U;
  }

  g_esp8266_last_init_status = ESP8266_INIT_STATUS_OK;
  return 1U;
}

void ESP8266_RxFeedByte(uint8_t byte)
{
  uint16_t count = g_esp8266_rx_count;

  if (count >= (ESP8266_RX_BUFFER_SIZE - 1U))
  {
    g_esp8266_rx_count = 0U;
    count = 0U;
  }

  g_esp8266_rx_buffer[count] = byte;
  count++;
  g_esp8266_rx_buffer[count] = 0U;
  g_esp8266_rx_count = count;
  g_esp8266_rx_last_tick = HAL_GetTick();

  if ((ESP8266_BufferContains(g_esp8266_rx_buffer, count, "CLOSED") != 0U) ||
      (ESP8266_BufferContains(g_esp8266_rx_buffer, count, "WIFI DISCONNECT") != 0U) ||
      (ESP8266_BufferContains(g_esp8266_rx_buffer, count, "link is not") != 0U) ||
      (ESP8266_BufferContains(g_esp8266_rx_buffer, count, "CONNECT FAIL") != 0U))
  {
    g_esp8266_transport_error = 1U;
  }
}

uint8_t ESP8266_HasTransportError(void)
{
  return g_esp8266_transport_error;
}

void ESP8266_ClearTransportError(void)
{
  g_esp8266_transport_error = 0U;
}

uint8_t ESP8266_GetLastInitStatus(void)
{
  return g_esp8266_last_init_status;
}

void ESP8266_DebugDumpCurrentRx(const char *tag, uint16_t limit)
{
  uint8_t snapshot[64];
  uint16_t count = 0U;
  uint16_t copy_len = 0U;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  count = g_esp8266_rx_count;
  copy_len = count;
  if (copy_len > sizeof(snapshot))
  {
    copy_len = (uint16_t)sizeof(snapshot);
  }
  memcpy(snapshot, g_esp8266_rx_buffer, copy_len);
  if (primask == 0U)
  {
    __enable_irq();
  }

  Debug_Printf("[TCP] RX SNAPSHOT tag=%s count=%u\r\n",
               (tag != NULL) ? tag : "-",
               (unsigned int)count);
  ESP8266_DebugPrintHexPrefix(snapshot, copy_len, limit);
}
