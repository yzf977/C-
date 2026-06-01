#include "key_handler.h"
#include <rtdevice.h>
#include <board.h>

static const rt_uint8_t key_pins[] = {
    GET_PIN(D, 1),  // KEY1
    GET_PIN(D, 2),  // KEY2
    GET_PIN(D, 3),  // KEY3
    GET_PIN(D, 4),  // KEY4
    GET_PIN(D, 5),  // KEY5
    GET_PIN(E, 7),  // KEY6
    GET_PIN(E, 8),  // KEY7
    GET_PIN(E, 9)   // KEY8
};

static void (*callbacks[KEY_NUMS])(void) = {0};
static uint8_t pressed[KEY_NUMS] = {0};
static uint32_t last_time[KEY_NUMS] = {0};

void key_init(void)
{
    for (int i = 0; i < KEY_NUMS; i++) {
        rt_pin_mode(key_pins[i], PIN_MODE_INPUT_PULLUP);
    }
    rt_kprintf("Key initialized, %d keys\n", KEY_NUMS);
}

void key_scan(void)
{
    uint32_t now = rt_tick_get();
    for (int i = 0; i < KEY_NUMS; i++) {
        uint8_t state = rt_pin_read(key_pins[i]);
        if (state == 0 && !pressed[i]) {
            rt_thread_mdelay(10);
            if (rt_pin_read(key_pins[i]) == 0) {
                pressed[i] = 1;
                last_time[i] = now;
            }
        } else if (state == 1 && pressed[i]) {
            rt_thread_mdelay(10);
            if (rt_pin_read(key_pins[i]) == 1) {
                pressed[i] = 0;
                if ((now - last_time[i]) > 20 && (now - last_time[i]) < 1000) {
                    if (callbacks[i]) {
                        callbacks[i]();
                    }
                }
            }
        }
    }
}

void key_register_callback(key_id_t id, void (*callback)(void))
{
    if (id < KEY_NUMS) {
        callbacks[id] = callback;
    }
}
