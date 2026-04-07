#ifndef __UART_SCREEN_H
#define __UART_SCREEN_H

#include "stm32h7xx_hal.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  SCREEN_TYPE_NEXTION = 0U
} ScreenType_t;

typedef enum
{
  SCREEN_BAUD_115200 = 115200U
} ScreenBaud_t;

void Screen_Init(UART_HandleTypeDef *huart, ScreenType_t type, uint32_t baudrate);
uint8_t Screen_IsReady(void);

void Screen_SendBytes(const uint8_t *data, uint16_t len);
void Screen_SendString(const char *str);
void Screen_SendCommand(const char *cmd);

void Screen_Nextion_SetPage(uint8_t page_id);
void Screen_Nextion_SetText(const char *component, const char *text);
void Screen_Nextion_SetValue(const char *component, int32_t value);
void Screen_Nextion_SetPicture(const char *component, uint16_t picture_id);

#ifdef __cplusplus
}
#endif

#endif /* __UART_SCREEN_H */
