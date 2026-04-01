#ifndef __DEBUG_UART7_H
#define __DEBUG_UART7_H

#include <stdint.h>

void Debug_Printf(const char *fmt, ...);
void Debug_LogEspTx(const char *line);
void Debug_LogEspRx(const uint8_t *data, uint16_t len);
void Debug_LogBio(int32_t heart_rate, int32_t spo2, int8_t hr_valid, int8_t spo2_valid);

#endif /* __DEBUG_UART7_H */
