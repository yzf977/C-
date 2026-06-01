/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-03-13     yzf       the first version
 */
#ifndef __TIM2_TRIGGER_H__
#define __TIM2_TRIGGER_H__

#include <stm32f4xx_hal.h>

extern TIM_HandleTypeDef htim2;
void MX_TIM2_Init(void);
void tim2_set_sample_rate(uint32_t khz);  // 设置采样率，单位kHz

#endif
