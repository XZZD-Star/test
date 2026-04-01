#ifndef MOTION_WINDOW_TEST_H
#define MOTION_WINDOW_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void MotionWindowTest_Init(void);
void MotionWindowTest_Reset(void);
void MotionWindowTest_RequestRun(void);
uint8_t MotionWindowTest_RunPending(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_WINDOW_TEST_H */
