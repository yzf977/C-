#ifndef __WS2812B_SPI_H__
#define __WS2812B_SPI_H__

#include <rtthread.h>
#include <stdint.h>

#define WS2812B_NUMS        256     // 8x32 = 256

#define WS2812B_BLACK       0x000000
#define WS2812B_RED         0x00FF00   // GRB
#define WS2812B_GREEN       0xFF0000
#define WS2812B_BLUE        0x0000FF
#define WS2812B_WHITE       0xFFFFFF

/* 灯带模式枚举（增加 LOW 和 HIGH） */
typedef enum {
    LED_MODE_FLOW = 0,
    LED_MODE_RAINBOW,
    LED_MODE_BREATH,
    LED_MODE_SPECTRUM_COLUMN,   // 原COL模式
    LED_MODE_SPECTRUM_DUAL,     // 双色点阵
    LED_MODE_SPECTRUM_ROLL,     // 瀑布
    LED_MODE_SPECTRUM_LOW,      // 低频模式
    LED_MODE_SPECTRUM_HIGH,     // 高频模式
    LED_MODE_COUNT
} led_mode_t;

/* 全局变量（供外部访问） */
extern uint8_t global_brightness;
extern led_mode_t current_led_mode;

/* 基本函数 */
int ws2812b_init(void);
void ws2812b_set_color(uint16_t index, uint32_t color);
void ws2812b_show(void);
void ws2812b_clear(void);
void ws2812b_set_all(uint32_t color);

/* 功能函数 */
void ws2812b_set_brightness(uint8_t brightness);
void ws2812b_set_mode(led_mode_t mode);
void ws2812b_update(void);   // 根据当前模式更新灯带

#endif
