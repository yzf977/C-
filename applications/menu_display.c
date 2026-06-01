#include "menu_display.h"
#include "ssd1306.h"
#include "spectrum_display.h"
#include "ws2812b_spi.h"
#include <stdio.h>
#include <string.h>

static char line_buf[20];
extern uint32_t sample_rate_khz;   // 引用 main.c 中的采样率变量

static const char* led_mode_str(led_mode_t mode)
{
    switch (mode) {
        case LED_MODE_FLOW: return "FLOW";
        case LED_MODE_RAINBOW: return "RAINBOW";
        case LED_MODE_BREATH: return "BREATH";
        case LED_MODE_SPECTRUM_COLUMN: return "COL";
        case LED_MODE_SPECTRUM_DUAL: return "DUAL";
        case LED_MODE_SPECTRUM_ROLL: return "ROLL";
        case LED_MODE_SPECTRUM_LOW: return "LOW";
        case LED_MODE_SPECTRUM_HIGH: return "HIGH";
        default: return "UNK";
    }
}

static const char* spc_mode_str(spectrum_mode_t mode)
{
    switch (mode) {
        case SPC_MODE_BAR: return "BAR";
        case SPC_MODE_PEAK_HOLD: return "PEAK";
        case SPC_MODE_DOT_MATRIX: return "DOT";
        default: return "BAR";
    }
}

void menu_init(void)
{
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("Menu", Font_6x8, White);
    ssd1306_UpdateScreen();
    rt_thread_mdelay(500);
    menu_update();
}

void menu_update(void)
{
    ssd1306_Fill(Black);

    // 左侧：灯带信息
    ssd1306_SetCursor(0, 0);
    sprintf(line_buf, "LED:%s", led_mode_str(current_led_mode));
    ssd1306_WriteString(line_buf, Font_6x8, White);

    ssd1306_SetCursor(0, 8);
    sprintf(line_buf, "BRT:%3d%%", global_brightness);
    ssd1306_WriteString(line_buf, Font_6x8, White);

    // 右侧：频谱信息
    ssd1306_SetCursor(64, 0);
    sprintf(line_buf, "SPC:%s", spc_mode_str(current_mode));
    ssd1306_WriteString(line_buf, Font_6x8, White);

    ssd1306_SetCursor(64, 8);
    sprintf(line_buf, "SEN:%3d", (int)(sensitivity * 100));  // 灵敏度也改为三位
    ssd1306_WriteString(line_buf, Font_6x8, White);

    ssd1306_SetCursor(64, 16);
    sprintf(line_buf, "GAIN:%3d%%", (int)(gain * 100));        // 增益显示为百分比
    ssd1306_WriteString(line_buf, Font_6x8, White);

    ssd1306_SetCursor(64, 24);
    sprintf(line_buf, "THR:%3d", noise_threshold);
    ssd1306_WriteString(line_buf, Font_6x8, White);

    ssd1306_SetCursor(64, 32);
    sprintf(line_buf, "SR:%2dk", sample_rate_khz);
    ssd1306_WriteString(line_buf, Font_6x8, White);

    // 底部按键提示
    ssd1306_SetCursor(2, 48);
    ssd1306_WriteString("1SPC 2LED 3BRT 4SEN", Font_6x8, White);
    ssd1306_SetCursor(2, 56);
    ssd1306_WriteString("5GAIN 6THR 7RST 8SR", Font_6x8, White);

    ssd1306_UpdateScreen();
}
