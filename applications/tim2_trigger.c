#include "tim2_trigger.h"

TIM_HandleTypeDef htim2;

void MX_TIM2_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;                // 不分频，计数频率84MHz
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1749;                // 84MHz/1750 = 48kHz
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim2);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);
}

void tim2_set_sample_rate(uint32_t khz)
{
    uint32_t period;
    if (khz < 10) khz = 10;
    if (khz > 48) khz = 48;
    period = 84000 / khz - 1;  // 84MHz = 84000 kHz

    // 停止定时器
    HAL_TIM_Base_Stop(&htim2);
    // 修改自动重载值
    __HAL_TIM_SET_AUTORELOAD(&htim2, period);
    // 重置计数器
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    // 重新启动定时器
    HAL_TIM_Base_Start(&htim2);
}
