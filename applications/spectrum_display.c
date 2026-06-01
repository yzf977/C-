#include "spectrum_display.h"
#include "board.h"
#include <string.h>
#include "stm32f4xx_hal.h"
#include <math.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_BUFFER_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

static SPI_HandleTypeDef hspi1;
static uint8_t framebuffer[OLED_BUFFER_SIZE];

#define OLED_CS_PIN   GPIO_PIN_14
#define OLED_CS_PORT  GPIOF
#define OLED_DC_PIN   GPIO_PIN_13
#define OLED_DC_PORT  GPIOF
#define OLED_RES_PIN  GPIO_PIN_15
#define OLED_RES_PORT GPIOF

float sensitivity = 1.0f;
uint8_t noise_threshold = 0;
uint8_t fps = 60;
float gain = 1.0f;                     // 定义增益变量
spectrum_mode_t current_mode = SPC_MODE_BAR;

static float peak_bands[FREQ_BANDS];
static const float PEAK_DECAY = 0.85f;
static uint8_t dot_matrix_history[FREQ_BANDS][64];
static uint8_t dot_matrix_col = 0;

static uint32_t rand_seed = 1;
static uint32_t fast_rand(void)
{
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7FFF;
}

/* 发送命令 */
static void oled_write_cmd(uint8_t cmd)
{
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
}

/* 批量发送数据 */
static void oled_write_data_bulk(uint8_t *data, uint16_t len)
{
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, data, len, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
}

/* 硬件初始化 */
static void oled_hw_init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = OLED_CS_PIN | OLED_DC_PIN | OLED_RES_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_RES_PORT, OLED_RES_PIN, GPIO_PIN_SET);

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;   // CPOL=1
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;        // CPHA=1 (模式3)
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; // 5.25MHz
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi1);
}

/* SSD1306 初始化序列 */
static void oled_init_display(void)
{
    HAL_GPIO_WritePin(OLED_RES_PORT, OLED_RES_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(OLED_RES_PORT, OLED_RES_PIN, GPIO_PIN_SET);
    HAL_Delay(10);

    oled_write_cmd(0xAE);
    oled_write_cmd(0xD5); oled_write_cmd(0x80);
    oled_write_cmd(0xA8); oled_write_cmd(0x3F);
    oled_write_cmd(0xD3); oled_write_cmd(0x00);
    oled_write_cmd(0x40);
    oled_write_cmd(0x8D); oled_write_cmd(0x14);
    oled_write_cmd(0x20); oled_write_cmd(0x00);
    oled_write_cmd(0xA1);
    oled_write_cmd(0xC8);
    oled_write_cmd(0xDA); oled_write_cmd(0x12);
    oled_write_cmd(0x81); oled_write_cmd(0xCF);
    oled_write_cmd(0xD9); oled_write_cmd(0xF1);
    oled_write_cmd(0xDB); oled_write_cmd(0x40);
    oled_write_cmd(0xA4);
    oled_write_cmd(0xA6);
    oled_write_cmd(0x2E);
    oled_write_cmd(0xAF);
}

/* 清屏 */
static void oled_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
    for (uint8_t page = 0; page < 8; page++) {
        oled_write_cmd(0xB0 + page);
        oled_write_cmd(0x00);
        oled_write_cmd(0x10);
        oled_write_data_bulk(framebuffer + page * 128, 128);
    }
}

/* 画点 */
static void oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    uint16_t idx = (y / 8) * OLED_WIDTH + x;
    uint8_t bit = y % 8;
    if (color)
        framebuffer[idx] |= (1 << bit);
    else
        framebuffer[idx] &= ~(1 << bit);
}

/* 更新屏幕 */
static void oled_update(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        oled_write_cmd(0xB0 + page);
        oled_write_cmd(0x00);
        oled_write_cmd(0x10);
        oled_write_data_bulk(framebuffer + page * 128, 128);
    }
}

/* 绘制柱状图模式（应用增益） */
static void draw_bar_mode(const float *bands)
{
    memset(framebuffer, 0, sizeof(framebuffer));
    for (int i = 0; i < FREQ_BANDS; i++) {
        float val = bands[i] * gain;
        if (val > 100) val = 100;
        int height = (int)(val * (OLED_HEIGHT - 1) / 100.0f);
        if (height > OLED_HEIGHT - 1) height = OLED_HEIGHT - 1;
        if (height < 1) height = 1;
        int x = i * 2;
        for (int y = 0; y < height; y++) {
            oled_draw_pixel(x, OLED_HEIGHT - 1 - y, 1);
            oled_draw_pixel(x + 1, OLED_HEIGHT - 1 - y, 1);
        }
    }
    oled_update();
}

/* 绘制峰值保持模式（应用增益） */
static void draw_peak_hold_mode(const float *bands)
{
    memset(framebuffer, 0, sizeof(framebuffer));
    for (int i = 0; i < FREQ_BANDS; i++) {
        float val = bands[i] * gain;
        if (val > 100) val = 100;
        int height = (int)(val * (OLED_HEIGHT - 1) / 100.0f);
        if (height > OLED_HEIGHT - 1) height = OLED_HEIGHT - 1;
        if (height < 1) height = 1;
        int x = i * 2;
        for (int y = 0; y < height; y++) {
            oled_draw_pixel(x, OLED_HEIGHT - 1 - y, 1);
            oled_draw_pixel(x + 1, OLED_HEIGHT - 1 - y, 1);
        }

        if (bands[i] > peak_bands[i])
            peak_bands[i] = bands[i];
        else
            peak_bands[i] *= PEAK_DECAY;

        int peak_y = OLED_HEIGHT - 1 - (int)(peak_bands[i] * (OLED_HEIGHT - 1) / 100.0f);
        if (peak_y < 0) peak_y = 0;
        if (peak_y < OLED_HEIGHT - 1) {
            oled_draw_pixel(x, peak_y, 1);
            oled_draw_pixel(x + 1, peak_y, 1);
            oled_draw_pixel(x, peak_y + 1, 1);
            oled_draw_pixel(x + 1, peak_y + 1, 1);
        }
    }
    oled_update();
}

/* 绘制点阵模式（应用增益） */
static void draw_dot_matrix_mode(const float *bands)
{
    for (int i = 0; i < FREQ_BANDS; i++) {
        uint8_t val = (uint8_t)(bands[i] * gain + 0.5f);
        if (val > 100) val = 100;
        dot_matrix_history[i][dot_matrix_col] = val;
    }
    dot_matrix_col = (dot_matrix_col + 1) % 64;

    memset(framebuffer, 0, sizeof(framebuffer));
    for (int col = 0; col < 64; col++) {
        int data_col = (dot_matrix_col - 1 - col + 64) % 64;
        for (int row = 0; row < FREQ_BANDS; row++) {
            uint8_t energy = dot_matrix_history[row][data_col];
            int dots = energy * 4 / 100;
            if (dots > 4) dots = 4;
            int base_x = col * 2;
            int base_y = OLED_HEIGHT - 1 - row;
            for (int d = 0; d < dots; d++) {
                int dx = fast_rand() % 3 - 1;
                int dy = fast_rand() % 3 - 1;
                int px = base_x + dx;
                int py = base_y + dy;
                if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT)
                    oled_draw_pixel(px, py, 1);
            }
        }
    }
    oled_update();
}

/* 初始化频谱显示模块 */
void spectrum_init(void)
{
    oled_hw_init();
    oled_init_display();
    oled_clear();
    memset(peak_bands, 0, sizeof(peak_bands));
    memset(dot_matrix_history, 0, sizeof(dot_matrix_history));
    dot_matrix_col = 0;
    rt_kprintf("Spectrum display initialized\n");
}

/* 设置显示模式 */
void spectrum_set_mode(spectrum_mode_t mode)
{
    if (mode < SPC_MODE_COUNT) {
        current_mode = mode;
        if (mode == SPC_MODE_PEAK_HOLD)
            memset(peak_bands, 0, sizeof(peak_bands));
        else if (mode == SPC_MODE_DOT_MATRIX) {
            memset(dot_matrix_history, 0, sizeof(dot_matrix_history));
            dot_matrix_col = 0;
        }
    }
}

/* 获取当前模式 */
spectrum_mode_t spectrum_get_mode(void)
{
    return current_mode;
}

/* 绘制频谱（根据当前模式） */
void spectrum_draw(const fft_result_t *result)
{
    switch (current_mode) {
        case SPC_MODE_BAR:
            draw_bar_mode(result->bands);
            break;
        case SPC_MODE_PEAK_HOLD:
            draw_peak_hold_mode(result->bands);
            break;
        case SPC_MODE_DOT_MATRIX:
            draw_dot_matrix_mode(result->bands);
            break;
        default:
            draw_bar_mode(result->bands);
            break;
    }
}

/* 参数设置函数 */
void spectrum_set_sensitivity(float sens) { sensitivity = sens; }
void spectrum_set_noise_threshold(uint8_t thr) { noise_threshold = thr; }
void spectrum_set_fps(uint8_t f) { fps = f; }
void spectrum_set_gain(float g) { gain = g; }
