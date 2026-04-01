#include "debug_uart7.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "usart.h"

#define DEBUG_UART7_BUFFER_SIZE 256U

static void Debug_Uart7_Write(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U) || (huart7.Instance == NULL))
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart7, (uint8_t *)data, len, HAL_MAX_DELAY);
}

static void Debug_Write(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    return;
  }

  Debug_Uart7_Write(data, len);
}

static void Debug_Uart7_WriteString(const char *text)
{
  if (text == NULL)
  {
    return;
  }

  Debug_Write((const uint8_t *)text, (uint16_t)strlen(text));
}

void Debug_Printf(const char *fmt, ...)
{
  char buffer[DEBUG_UART7_BUFFER_SIZE];
  int length = 0;
  va_list args;

  if (fmt == NULL)
  {
    return;
  }

  va_start(args, fmt);
  length = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (length <= 0)
  {
    return;
  }

  if ((size_t)length >= sizeof(buffer))
  {
    length = (int)(sizeof(buffer) - 1U);
  }

  Debug_Write((const uint8_t *)buffer, (uint16_t)length);
}

void Debug_LogEspTx(const char *line)
{
  if ((line == NULL) || (*line == '\0'))
  {
    Debug_Printf("ESP>> <empty>\r\n");
    return;
  }

  Debug_Printf("ESP>> %s\r\n", line);
}

void Debug_LogEspRx(const uint8_t *data, uint16_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    Debug_Printf("ESP<< <empty>\r\n");
    return;
  }

  Debug_Uart7_WriteString("ESP<< ");
  Debug_Write(data, len);

  if (data[len - 1U] != '\n')
  {
    Debug_Uart7_WriteString("\r\n");
  }
}

void Debug_LogBio(int32_t heart_rate, int32_t spo2, int8_t hr_valid, int8_t spo2_valid)
{
  Debug_Printf("BIO>> HR=%ld SPO2=%ld HR_VALID=%d SPO2_VALID=%d\r\n",
               (long)heart_rate,
               (long)spo2,
               (int)hr_valid,
               (int)spo2_valid);
}
