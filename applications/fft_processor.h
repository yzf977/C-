#ifndef __FFT_PROCESSOR_H__
#define __FFT_PROCESSOR_H__

#include <rtthread.h>
#include <stdint.h>

#define FFT_SIZE        512
#define FREQ_BANDS      64

typedef struct {
    float bands[FREQ_BANDS];
    uint32_t timestamp;
} fft_result_t;

int fft_init(void);
void fft_process(int16_t *audio_data, fft_result_t *result, float sensitivity, uint8_t noise_threshold);

#endif
