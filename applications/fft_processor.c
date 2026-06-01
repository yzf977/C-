#include "fft_processor.h"
#include <arm_math.h>
#include <math.h>
#include <string.h>

static arm_rfft_fast_instance_f32 fft_inst;
static float32_t window[FFT_SIZE];
static uint8_t fft_ready = 0;
static uint8_t band_map[FREQ_BANDS][2];
static float32_t input[FFT_SIZE];
static float32_t output[FFT_SIZE];
static float32_t mag[FFT_SIZE/2];

/* 动态基准相关 */
static float peak_db = -80.0f;           // 历史最大dB值
static const float PEAK_DECAY = 0.995f;  // 每帧衰减系数（平滑因子）

static void init_band_map(void)
{
    float min_freq = 20.0f, max_freq = 24000.0f, freq_res = 48000.0f / FFT_SIZE;
    for (int i = 0; i < FREQ_BANDS; i++) {
        if (i == 0) {
            band_map[i][0] = 0;
            band_map[i][1] = (uint8_t)(min_freq / freq_res);
        } else {
            float freq = min_freq * powf(max_freq / min_freq, (float)i / (FREQ_BANDS - 1));
            int idx = (int)(freq / freq_res);
            if (idx < band_map[i-1][1] + 1) idx = band_map[i-1][1] + 1;
            if (idx >= FFT_SIZE/2) idx = FFT_SIZE/2 - 1;
            band_map[i][0] = band_map[i-1][1] + 1;
            band_map[i][1] = idx;
        }
        if (band_map[i][0] > FFT_SIZE/2-1) band_map[i][0] = FFT_SIZE/2-1;
        if (band_map[i][1] > FFT_SIZE/2-1) band_map[i][1] = FFT_SIZE/2-1;
        if (band_map[i][1] < band_map[i][0]) band_map[i][1] = band_map[i][0];
    }
}

static void init_window(void)
{
    for (uint16_t i = 0; i < FFT_SIZE; i++)
        window[i] = 0.5f * (1.0f - cosf(2 * 3.1415926f * i / (FFT_SIZE - 1)));
}

int fft_init(void)
{
    if (fft_ready) return RT_EOK;
    if (arm_rfft_fast_init_f32(&fft_inst, FFT_SIZE) != ARM_MATH_SUCCESS)
        return -RT_ERROR;
    init_window();
    init_band_map();
    fft_ready = 1;
    return RT_EOK;
}

void fft_process(int16_t *audio_data, fft_result_t *result, float sensitivity, uint8_t noise_threshold)
{
    if (!fft_ready || !audio_data || !result) return;

    // 应用灵敏度
    for (uint16_t i = 0; i < FFT_SIZE; i++) {
        input[i] = (float32_t)audio_data[i] / 32768.0f * window[i] * sensitivity;
    }

    arm_rfft_fast_f32(&fft_inst, input, output, 0);
    arm_cmplx_mag_f32(output, mag, FFT_SIZE/2);

    for (uint16_t i = 0; i < FFT_SIZE/2; i++) {
        if (i == 0) mag[i] /= FFT_SIZE;
        else mag[i] *= 2.0f / FFT_SIZE;
    }

    // 计算各频段dB值，并找出本帧最大值
    float db_values[FREQ_BANDS];
    float max_db_this_frame = -80.0f;
    for (int b = 0; b < FREQ_BANDS; b++) {
        int start = band_map[b][0], end = band_map[b][1], cnt = end - start + 1;
        float sum = 0;
        for (int k = start; k <= end; k++) sum += mag[k];
        float avg = sum / cnt;
        float db = 20.0f * log10f(avg + 1e-6f);
        db_values[b] = db;
        if (db > max_db_this_frame) max_db_this_frame = db;
    }

    // 更新峰值（采用慢速衰减）
    if (max_db_this_frame > peak_db) {
        peak_db = max_db_this_frame;
    } else {
        peak_db = peak_db * PEAK_DECAY + max_db_this_frame * (1 - PEAK_DECAY); // 指数平滑
    }
    if (peak_db < -80.0f) peak_db = -80.0f;  // 下限保护

    // 归一化到0~100，动态基准从-80到peak_db
    float range = peak_db - (-80.0f);
    if (range < 1.0f) range = 1.0f;  // 防止除零
    for (int b = 0; b < FREQ_BANDS; b++) {
        float norm = (db_values[b] + 80.0f) * 100.0f / range;
        if (norm < 0) norm = 0;
        if (norm > 100) norm = 100;
        result->bands[b] = norm;
    }

    // 应用噪声门限
    for (int b = 0; b < FREQ_BANDS; b++) {
        if (result->bands[b] < noise_threshold) result->bands[b] = 0;
    }

    result->timestamp = rt_tick_get();
}
