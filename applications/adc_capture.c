#include "adc_capture.h"
#include "tim2_trigger.h"
#include "fft_processor.h"
#include "spectrum_display.h"
#include <rtthread.h>
#include <rthw.h>
#include <stm32f4xx_hal.h>

#define FFT_SIZE            512

static ADC_HandleTypeDef hadc1;
uint16_t sample_buffer[FFT_SIZE] __attribute__((aligned(32)));
static volatile uint32_t sample_index = 0;
static rt_event_t frame_event;
static uint32_t frame_count = 0;
static uint32_t frame_ready_cnt = 0;

fft_result_t shared_result;
rt_sem_t data_ready_sem;

void TIM2_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            uint16_t val = HAL_ADC_GetValue(&hadc1);
            sample_buffer[sample_index++] = val;
            if (sample_index >= FFT_SIZE) {
                sample_index = 0;
                frame_ready_cnt++;
                rt_event_send(frame_event, 0x01);
            }
        }
    }
}

static void hw_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    ADC_ChannelConfTypeDef sConfig = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

static void audio_thread_entry(void *param)
{
    rt_uint32_t recv_set;
    int16_t pcm_buffer[FFT_SIZE];

    HAL_TIM_Base_Start_IT(&htim2);

    while (1) {
        if (rt_event_recv(frame_event, 0x01, RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR,
                          RT_WAITING_FOREVER, &recv_set) == RT_EOK) {
            frame_count++;
            // 将ADC值转换为PCM
            for (int i = 0; i < FFT_SIZE; i++)
                pcm_buffer[i] = (int16_t)(sample_buffer[i] - 2048);
            // 执行FFT，结果存入 shared_result
            fft_process(pcm_buffer, &shared_result, sensitivity, noise_threshold);
            rt_sem_release(data_ready_sem);
        }
    }
}

int adc_capture_init(void)
{
    frame_event = rt_event_create("frame_event", RT_IPC_FLAG_FIFO);
    if (!frame_event) return -RT_ENOMEM;

    data_ready_sem = rt_sem_create("data_ready", 0, RT_IPC_FLAG_FIFO);
    if (!data_ready_sem) { rt_event_delete(frame_event); return -RT_ENOMEM; }

    hw_init();

    rt_thread_t tid = rt_thread_create("audio_cap", audio_thread_entry, RT_NULL,
                                       8192, 18, 10);
    if (!tid) {
        rt_sem_delete(data_ready_sem);
        rt_event_delete(frame_event);
        return -RT_ENOMEM;
    }
    rt_thread_startup(tid);
    return RT_EOK;
}
