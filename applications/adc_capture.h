#ifndef __ADC_CAPTURE_H__
#define __ADC_CAPTURE_H__

#include <rtthread.h>
#include <stdint.h>
#include "fft_processor.h"

extern uint16_t sample_buffer[FFT_SIZE];
extern fft_result_t shared_result;
extern rt_sem_t data_ready_sem;

int adc_capture_init(void);

#endif
