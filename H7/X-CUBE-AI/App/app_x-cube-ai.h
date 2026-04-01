
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_AI_H
#define __APP_AI_H
#ifdef __cplusplus
extern "C" {
#endif
/**
  ******************************************************************************
  * @file    app_x-cube-ai.h
  * @author  X-CUBE-AI C code generator
  * @brief   AI entry function definitions
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
/* Includes ------------------------------------------------------------------*/
#include "ai_platform.h"

#define APP_X_CUBE_AI_INPUT_FRAMES   (30U)
#define APP_X_CUBE_AI_INPUT_FEATURES (6U)
#define APP_X_CUBE_AI_INPUT_FLOATS   (APP_X_CUBE_AI_INPUT_FRAMES * APP_X_CUBE_AI_INPUT_FEATURES)
#define APP_X_CUBE_AI_OUTPUT_CLASSES (5U)

void MX_X_CUBE_AI_Init(void);
void MX_X_CUBE_AI_Process(void);
/* USER CODE BEGIN includes */
int AppXCubeAI_Run(
  const float input[APP_X_CUBE_AI_INPUT_FLOATS],
  float output[APP_X_CUBE_AI_OUTPUT_CLASSES]);
/* USER CODE END includes */
#ifdef __cplusplus
}
#endif
#endif /*__STMicroelectronics_X-CUBE-AI_10_2_0_H */
