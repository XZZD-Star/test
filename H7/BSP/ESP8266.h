#ifndef __ESP8266_H__
#define __ESP8266_H__

#include <stdint.h>

#define WIFI_SSID              "www"
#define WIFI_PASSWORD          "www123456"

#define ONENET_HOST            "mqtts.heclouds.com"
#define ONENET_PORT            1883U

#define ESP8266_INIT_STATUS_OK            0U
#define ESP8266_INIT_STATUS_FAIL_AT       1U
#define ESP8266_INIT_STATUS_FAIL_ATE0     2U
#define ESP8266_INIT_STATUS_FAIL_CWMODE   3U
#define ESP8266_INIT_STATUS_FAIL_DHCP     4U
#define ESP8266_INIT_STATUS_FAIL_CWJAP    5U
#define ESP8266_INIT_STATUS_FAIL_CIPMUX   6U
#define ESP8266_INIT_STATUS_FAIL_CIPSTART 7U

void ESP8266_Clear(void);
uint8_t ESP8266_WaitReceive(uint32_t timeout_ms);
uint8_t ESP8266_SendCmd(const char *cmd, const char *expect, uint32_t timeout_ms);
uint8_t ESP8266_SendData(const uint8_t *data, uint16_t len);
uint8_t *ESP8266_GetIPD(uint32_t timeout_ms);
uint8_t ESP8266_Init(void);

void ESP8266_RxFeedByte(uint8_t byte);
uint8_t ESP8266_HasTransportError(void);
void ESP8266_ClearTransportError(void);
uint8_t ESP8266_GetLastInitStatus(void);
void ESP8266_DebugDumpCurrentRx(const char *tag, uint16_t limit);

#endif /* __ESP8266_H__ */
