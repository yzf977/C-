#ifndef __SPECTRUM_DISPLAY_H__
#define __SPECTRUM_DISPLAY_H__

#include "fft_processor.h"

typedef enum {
    SPC_MODE_BAR = 0,
    SPC_MODE_PEAK_HOLD,
    SPC_MODE_DOT_MATRIX,
    SPC_MODE_COUNT
} spectrum_mode_t;

/* 全局变量声明 */
extern float sensitivity;
extern uint8_t noise_threshold;
extern uint8_t fps;
extern float gain;               // 增益变量
extern spectrum_mode_t current_mode;

void spectrum_init(void);
void spectrum_set_mode(spectrum_mode_t mode);
spectrum_mode_t spectrum_get_mode(void);
void spectrum_draw(const fft_result_t *result);
void spectrum_set_sensitivity(float sens);
void spectrum_set_noise_threshold(uint8_t thr);
void spectrum_set_fps(uint8_t fps);
void spectrum_set_gain(float g);   // 设置增益

#endif
