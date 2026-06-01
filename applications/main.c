#include <rtthread.h>
#include <string.h>
#include "adc_capture.h"
#include "fft_processor.h"
#include "ws2812b_spi.h"
#include "tim2_trigger.h"       // 包含定时器函数
#include "menu_display.h"
#include "key_handler.h"
#include "spectrum_display.h"

/* 全局变量声明 */
uint32_t sample_rate_khz = 48;   // 默认采样率 48kHz

/* 按键回调声明 */
static void key1_cb(void);
static void key2_cb(void);
static void key3_cb(void);
static void key4_cb(void);
static void key5_cb(void);
static void key6_cb(void);
static void key7_cb(void);
static void key8_cb(void);

/* 频谱处理线程（使用共享结果） */
static void spectrum_proc_thread(void *param)
{
    uint32_t cnt = 0;

    while (1) {
        if (rt_sem_take(data_ready_sem, RT_WAITING_FOREVER) == RT_EOK) {
            spectrum_draw(&shared_result);
            cnt++;
            if (cnt % 500 == 0)
                rt_kprintf("Spectrum processed %d\n", cnt);
        }
    }
}

/* 灯带更新线程 */
static void led_thread_entry(void *param)
{
    uint32_t led_cnt = 0;
    ws2812b_init();

    while (1) {
        led_cnt++;
        ws2812b_update();
        if (led_cnt % 300 == 0)
            rt_kprintf("LED update %d\n", led_cnt);
        rt_thread_mdelay(1000 / 60);
    }
}

/* 菜单更新线程 */
static void menu_thread_entry(void *param)
{
    while (1) {
        menu_update();
        rt_thread_mdelay(500);
    }
}

/* 按键扫描线程 */
static void key_scan_thread(void *param)
{
    while (1) {
        key_scan();
        rt_thread_mdelay(10);
    }
}

int main(void)
{
    MX_TIM2_Init();              // 初始化定时器，默认48kHz
    fft_init();
    adc_capture_init();
    key_init();
    spectrum_init();
    menu_init();

    key_register_callback(KEY1, key1_cb);
    key_register_callback(KEY2, key2_cb);
    key_register_callback(KEY3, key3_cb);
    key_register_callback(KEY4, key4_cb);
    key_register_callback(KEY5, key5_cb);
    key_register_callback(KEY6, key6_cb);
    key_register_callback(KEY7, key7_cb);
    key_register_callback(KEY8, key8_cb);

    rt_thread_t tid;

    tid = rt_thread_create("spectrum_proc", spectrum_proc_thread, RT_NULL,
                           4096, 15, 10);
    if (tid) rt_thread_startup(tid);

    tid = rt_thread_create("led_show", led_thread_entry, RT_NULL,
                           2048, 12, 10);
    if (tid) rt_thread_startup(tid);

    tid = rt_thread_create("menu_update", menu_thread_entry, RT_NULL,
                           2048, 18, 10);
    if (tid) rt_thread_startup(tid);

    tid = rt_thread_create("key_scan", key_scan_thread, RT_NULL,
                           1024, 3, 10);
    if (tid) rt_thread_startup(tid);

    while (1) rt_thread_mdelay(1000);
    return 0;
}

/* 按键回调函数 */
static void key1_cb(void)
{
    spectrum_mode_t new = (spectrum_get_mode() + 1) % SPC_MODE_COUNT;
    spectrum_set_mode(new);
    rt_kprintf("Spectrum mode -> %d\n", new);
}

static void key2_cb(void)
{
    led_mode_t new = (current_led_mode + 1) % LED_MODE_COUNT;
    ws2812b_set_mode(new);
    rt_kprintf("LED mode -> %d\n", new);
}

static void key3_cb(void)
{
    global_brightness += 5;
    if (global_brightness > 100) global_brightness = 0;
    ws2812b_set_brightness(global_brightness);
    rt_kprintf("Brightness = %d\n", global_brightness);
}

static void key4_cb(void)
{
    sensitivity += 0.1f;
    if (sensitivity > 2.0f) sensitivity = 0.5f;
    rt_kprintf("Sensitivity = %.1f\n", sensitivity);
}

static void key5_cb(void)
{
    gain += 0.1f;
    if (gain > 3.0f) gain = 0.5f;
    rt_kprintf("Gain = %.1f\n", gain);
}

static void key6_cb(void)
{
    noise_threshold += 2;
    if (noise_threshold > 50) noise_threshold = 0;
    rt_kprintf("Noise threshold = %d\n", noise_threshold);
}

static void key7_cb(void)
{
    spectrum_set_mode(SPC_MODE_BAR);
    ws2812b_set_mode(LED_MODE_FLOW);
    ws2812b_set_brightness(5);
    sensitivity = 1.0f;
    noise_threshold = 0;
    gain = 1.0f;
    sample_rate_khz = 48;
    tim2_set_sample_rate(sample_rate_khz);
    rt_kprintf("Reset to defaults (SR=%d)\n", sample_rate_khz);
}

static void key8_cb(void)
{
    sample_rate_khz += 2;
    if (sample_rate_khz > 48) sample_rate_khz = 10;
    tim2_set_sample_rate(sample_rate_khz);
    rt_kprintf("Sample rate = %d kHz\n", sample_rate_khz);
}
