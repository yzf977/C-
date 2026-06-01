#include "ws2812b_spi.h"
#include <rtthread.h>
#include <rthw.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "adc_capture.h"      // 包含 shared_result 声明
#include "spectrum_display.h"  // 包含 gain 声明
#include <math.h>

static SPI_HandleTypeDef hspi2;
static uint8_t led_buffer[WS2812B_NUMS * 24];
static uint32_t led_colors[WS2812B_NUMS];
static struct rt_mutex mutex;

/* 全局变量定义 */
uint8_t global_brightness = 5;      // 默认5%
led_mode_t current_led_mode = LED_MODE_FLOW;

static uint32_t flow_pos = 0;
static uint8_t breath_dir = 1;
static uint8_t breath_val = 0;
static uint32_t roll_offset = 0;   // 用于瀑布模式的滚动偏移

/* ==================== 快速随机数函数（提前定义） ==================== */
static uint32_t fast_rand(void)
{
    static uint32_t seed = 1;
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

/* ==================== 颜色函数 ==================== */
/* 通用 HSV 转 GRB（色相 0~360） */
static uint32_t hue_to_grb(float hue)
{
    float s = 1.0f, v = 1.0f;
    uint8_t r, g, b;
    int hi = (int)(hue / 60) % 6;
    float f = hue / 60 - hi;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    switch (hi) {
        case 0: r = v * 255; g = t * 255; b = p * 255; break;
        case 1: r = q * 255; g = v * 255; b = p * 255; break;
        case 2: r = p * 255; g = v * 255; b = t * 255; break;
        case 3: r = p * 255; g = q * 255; b = v * 255; break;
        case 4: r = t * 255; g = p * 255; b = v * 255; break;
        default:r = v * 255; g = p * 255; b = q * 255; break;
    }
    return (g << 16) | (r << 8) | b; // GRB格式
}

/* 暖色渐变（红 -> 黄 -> 绿），能量0~100映射色相0~60 */
static uint32_t energy_to_color_warm(float energy)
{
    float hue = energy * 0.6f; // 0~60
    return hue_to_grb(hue);
}

/* 冷色渐变（青 -> 蓝 -> 紫），能量0~100映射色相180~300 */
static uint32_t energy_to_color_cold(float energy)
{
    float hue = 180.0f + energy * 1.2f; // 180~300
    return hue_to_grb(hue);
}

/* 根据物理布局计算LED索引：8行，32列，蛇形 */
static uint16_t led_index(uint8_t col, uint8_t row)
{
    if (col % 2 == 0)  // 偶数列：从上到下（row 0~7）
        return col * 8 + row;
    else               // 奇数列：从下到上（row 7~0）
        return col * 8 + (7 - row);
}

/* 应用亮度 */
static uint32_t apply_brightness(uint32_t color, uint8_t brightness)
{
    uint8_t r = (color >> 8) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = color & 0xFF;
    r = (uint16_t)r * brightness / 100;
    g = (uint16_t)g * brightness / 100;
    b = (uint16_t)b * brightness / 100;
    return (g << 16) | (r << 8) | b;
}

/* 编码颜色为 SPI 码流 */
static void encode_color(uint32_t color, uint8_t *buf)
{
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t r = (color >> 8)  & 0xFF;
    uint8_t b = color & 0xFF;

    for (int i = 0; i < 8; i++) buf[7 - i] = (g & (1 << i)) ? 0xF0 : 0xC0;
    for (int i = 0; i < 8; i++) buf[15 - i] = (r & (1 << i)) ? 0xF0 : 0xC0;
    for (int i = 0; i < 8; i++) buf[23 - i] = (b & (1 << i)) ? 0xF0 : 0xC0;
}

/* 更新缓冲区（根据当前颜色和亮度） */
static void update_buffer(void)
{
    for (int i = 0; i < WS2812B_NUMS; i++) {
        uint32_t col = apply_brightness(led_colors[i], global_brightness);
        encode_color(col, &led_buffer[i * 24]);
    }
}

/* SPI2 硬件初始化 */
static void MX_SPI2_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; // 42MHz/8 = 5.25MHz
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi2);
}

/* 初始化 */
int ws2812b_init(void)
{
    rt_mutex_init(&mutex, "ws2812", RT_IPC_FLAG_FIFO);
    MX_SPI2_Init();
    memset(led_colors, 0, sizeof(led_colors));
    update_buffer();
    rt_kprintf("WS2812B HAL SPI initialized\n");
    return RT_EOK;
}

void ws2812b_set_color(uint16_t index, uint32_t color)
{
    if (index >= WS2812B_NUMS) return;
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    led_colors[index] = color;
    update_buffer();
    rt_mutex_release(&mutex);
}

void ws2812b_set_all(uint32_t color)
{
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    for (int i = 0; i < WS2812B_NUMS; i++) led_colors[i] = color;
    update_buffer();
    rt_mutex_release(&mutex);
}

void ws2812b_clear(void)
{
    ws2812b_set_all(0);
}

void ws2812b_show(void)
{
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    rt_base_t level = rt_hw_interrupt_disable();
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi2, led_buffer, sizeof(led_buffer), 1000);
    rt_hw_interrupt_enable(level);
    if (status != HAL_OK) {
        rt_kprintf("HAL_SPI_Transmit error: %d\n", status);
    }
    rt_thread_mdelay(1);
    rt_mutex_release(&mutex);
}

void ws2812b_set_brightness(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    global_brightness = brightness;
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    update_buffer();
    rt_mutex_release(&mutex);
}

/* ==================== 原有模式 ==================== */
static void led_mode_flow(void)
{
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    memset(led_colors, 0, sizeof(led_colors));
    led_colors[flow_pos] = WS2812B_RED;
    flow_pos = (flow_pos + 1) % WS2812B_NUMS;
    update_buffer();
    rt_mutex_release(&mutex);
}

static void led_mode_rainbow(void)
{
    static uint8_t hue = 0;
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    for (int i = 0; i < WS2812B_NUMS; i++) {
        uint8_t h = (hue + i * 256 / WS2812B_NUMS) % 256;
        uint32_t color;
        if (h < 85) {
            color = ((255 - h*3) << 16) | ((h*3) << 8);
        } else if (h < 170) {
            h -= 85;
            color = (0 << 16) | ((255 - h*3) << 8) | (h*3);
        } else {
            h -= 170;
            color = ((h*3) << 16) | (0 << 8) | (255 - h*3);
        }
        led_colors[i] = color;
    }
    hue += 2;
    update_buffer();
    rt_mutex_release(&mutex);
}

static void led_mode_breath(void)
{
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    uint32_t color = apply_brightness(WS2812B_RED, breath_val);
    for (int i = 0; i < WS2812B_NUMS; i++) led_colors[i] = color;
    if (breath_dir) {
        breath_val += 2;
        if (breath_val >= 100) { breath_val = 100; breath_dir = 0; }
    } else {
        breath_val -= 2;
        if (breath_val <= 0) { breath_val = 0; breath_dir = 1; }
    }
    update_buffer();
    rt_mutex_release(&mutex);
}

/* ==================== 原COL模式（每列两个频段） ==================== */
static void led_mode_spectrum_column(void)
{
    const float *bands = shared_result.bands;
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    memset(led_colors, 0, sizeof(led_colors));

    for (int col = 0; col < 32; col++) {
        int idx1 = col * 2;
        int idx2 = col * 2 + 1;
        float energy = (bands[idx1] > bands[idx2]) ? bands[idx1] : bands[idx2];
        energy *= gain;
        if (energy > 100) energy = 100;
        int height = (int)(energy * 8 / 100.0f);
        if (height > 8) height = 8;
        if (height < 0) height = 0;
        uint32_t color = hue_to_grb(energy * 2.4f);
        for (int row = 7; row > 7 - height; row--) {
            uint16_t idx = led_index(col, row);
            led_colors[idx] = color;
        }
    }
    update_buffer();
    rt_mutex_release(&mutex);
}

/* ==================== 双色点阵模式 ==================== */
static void led_mode_spectrum_dual(void)
{
    const float *bands = shared_result.bands;
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    memset(led_colors, 0, sizeof(led_colors));

    for (int col = 0; col < 32; col++) {
        int idx_low = col * 2;
        int idx_high = col * 2 + 1;

        float energy_low = bands[idx_low] * gain;
        if (energy_low > 100) energy_low = 100;
        int dots_low = (int)(energy_low * 4 / 100);

        float energy_high = bands[idx_high] * gain;
        if (energy_high > 100) energy_high = 100;
        int dots_high = (int)(energy_high * 4 / 100);

        uint32_t color_low = 0x00FF00; // 绿色
        uint32_t color_high = 0x0000FF; // 蓝色

        for (int d = 0; d < dots_low; d++) {
            int row = fast_rand() % 4;
            uint16_t idx = led_index(col, row);
            led_colors[idx] = color_low;
        }
        for (int d = 0; d < dots_high; d++) {
            int row = 4 + fast_rand() % 4;
            uint16_t idx = led_index(col, row);
            led_colors[idx] = color_high;
        }
    }
    update_buffer();
    rt_mutex_release(&mutex);
}

/* ==================== 瀑布模式 ==================== */
static void led_mode_spectrum_roll(void)
{
    const float *bands = shared_result.bands;
    static uint8_t history[32][8] = {0};
    static int col_ptr = 0;

    uint8_t compressed[8];
    for (int row = 0; row < 8; row++) {
        float sum = 0;
        for (int k = 0; k < 8; k++) {
            sum += bands[row * 8 + k];
        }
        float avg = sum / 8 * gain;
        if (avg > 100) avg = 100;
        compressed[row] = (uint8_t)(avg * 8 / 100);
    }

    for (int row = 0; row < 8; row++) {
        history[col_ptr][row] = compressed[row];
    }
    col_ptr = (col_ptr + 1) % 32;

    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    memset(led_colors, 0, sizeof(led_colors));

    for (int col = 0; col < 32; col++) {
        int data_col = (col_ptr - 1 - col + 32) % 32;
        for (int row = 0; row < 8; row++) {
            uint8_t energy = history[data_col][row];
            if (energy > 0) {
                uint32_t color = hue_to_grb(energy * 12.5f);
                uint16_t idx = led_index(col, 7 - row);
                led_colors[idx] = color;
            }
        }
    }
    update_buffer();
    rt_mutex_release(&mutex);
}

/* ==================== 低频模式（新增） ==================== */
static void led_mode_spectrum_low(void)
{
    const float *bands = shared_result.bands;
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    memset(led_colors, 0, sizeof(led_colors));

    for (int col = 0; col < 32; col++) {
        float energy = bands[col] * gain;
        if (energy > 100) energy = 100;
        int height = (int)(energy * 8 / 100.0f);
        if (height > 8) height = 8;
        if (height < 0) height = 0;
        uint32_t color = energy_to_color_warm(energy);
        for (int row = 7; row > 7 - height; row--) {
            uint16_t idx = led_index(col, row);
            led_colors[idx] = color;
        }
    }
    update_buffer();
    rt_mutex_release(&mutex);
}

/* ==================== 高频模式（新增） ==================== */
static void led_mode_spectrum_high(void)
{
    const float *bands = shared_result.bands;
    rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    memset(led_colors, 0, sizeof(led_colors));

    for (int col = 0; col < 32; col++) {
        float energy = bands[col + 32] * gain;
        if (energy > 100) energy = 100;
        int height = (int)(energy * 8 / 100.0f);
        if (height > 8) height = 8;
        if (height < 0) height = 0;
        uint32_t color = energy_to_color_cold(energy);
        for (int row = 7; row > 7 - height; row--) {
            uint16_t idx = led_index(col, row);
            led_colors[idx] = color;
        }
    }
    update_buffer();
    rt_mutex_release(&mutex);
}

/* 设置模式 */
void ws2812b_set_mode(led_mode_t mode)
{
    current_led_mode = mode;
    flow_pos = 0;
    breath_val = 0;
    breath_dir = 1;
    roll_offset = 0;
    rt_kprintf("LED mode set to %d\n", mode);
}

/* 更新灯带（根据当前模式） */
void ws2812b_update(void)
{
    switch (current_led_mode) {
        case LED_MODE_FLOW: led_mode_flow(); break;
        case LED_MODE_RAINBOW: led_mode_rainbow(); break;
        case LED_MODE_BREATH: led_mode_breath(); break;
        case LED_MODE_SPECTRUM_COLUMN: led_mode_spectrum_column(); break;
        case LED_MODE_SPECTRUM_DUAL: led_mode_spectrum_dual(); break;
        case LED_MODE_SPECTRUM_ROLL: led_mode_spectrum_roll(); break;
        case LED_MODE_SPECTRUM_LOW: led_mode_spectrum_low(); break;
        case LED_MODE_SPECTRUM_HIGH: led_mode_spectrum_high(); break;
        default: led_mode_flow(); break;
    }
    ws2812b_show();
}
