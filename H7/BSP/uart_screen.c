#include "uart_screen.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
  UART_HandleTypeDef *huart;
  ScreenType_t type;
  uint32_t baudrate;
} ScreenContext_t;

static ScreenContext_t g_screen_ctx = {0};

static void Screen_SendNextionEnd(void)
{
  static const uint8_t k_end_bytes[3] = {0xFFU, 0xFFU, 0xFFU};

  Screen_SendBytes(k_end_bytes, (uint16_t)sizeof(k_end_bytes));
}

void Screen_Init(UART_HandleTypeDef *huart, ScreenType_t type, uint32_t baudrate)
{
  g_screen_ctx.huart = huart;
  g_screen_ctx.type = type;
  g_screen_ctx.baudrate = baudrate;

  (void)g_screen_ctx.baudrate;
}

uint8_t Screen_IsReady(void)
{
  return ((g_screen_ctx.huart != NULL) && (g_screen_ctx.huart->Instance != NULL)) ? 1U : 0U;
}

void Screen_SendBytes(const uint8_t *data, uint16_t len)
{
  if ((Screen_IsReady() == 0U) || (data == NULL) || (len == 0U))
  {
    return;
  }

  (void)HAL_UART_Transmit(g_screen_ctx.huart, (uint8_t *)data, len, HAL_MAX_DELAY);
}

void Screen_SendString(const char *str)
{
  if (str == NULL)
  {
    return;
  }

  Screen_SendBytes((const uint8_t *)str, (uint16_t)strlen(str));
}

void Screen_SendCommand(const char *cmd)
{
  if (cmd == NULL)
  {
    return;
  }

  Screen_SendString(cmd);

  if (g_screen_ctx.type == SCREEN_TYPE_NEXTION)
  {
    Screen_SendNextionEnd();
  }
}

void Screen_Nextion_SetPage(uint8_t page_id)
{
  char command[24];

  (void)snprintf(command, sizeof(command), "page %u", (unsigned int)page_id);
  Screen_SendCommand(command);
}

void Screen_Nextion_SetText(const char *component, const char *text)
{
  char command[96];

  if ((component == NULL) || (text == NULL))
  {
    return;
  }

  (void)snprintf(command, sizeof(command), "%s.txt=\"%s\"", component, text);
  Screen_SendCommand(command);
}

void Screen_Nextion_SetValue(const char *component, int32_t value)
{
  char command[64];

  if (component == NULL)
  {
    return;
  }

  (void)snprintf(command, sizeof(command), "%s.val=%ld", component, (long)value);
  Screen_SendCommand(command);
}

void Screen_Nextion_SetPicture(const char *component, uint16_t picture_id)
{
  char command[64];

  if (component == NULL)
  {
    return;
  }

  (void)snprintf(command, sizeof(command), "%s.pic=%u", component, (unsigned int)picture_id);
  Screen_SendCommand(command);
}
