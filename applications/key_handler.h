#ifndef __KEY_HANDLER_H__
#define __KEY_HANDLER_H__

#include <rtthread.h>
#include <stdint.h>

#define KEY_NUMS 8

/* 按键ID枚举 */
typedef enum {
    KEY1 = 0,
    KEY2,
    KEY3,
    KEY4,
    KEY5,
    KEY6,
    KEY7,
    KEY8
} key_id_t;

/* 初始化按键 */
void key_init(void);

/* 扫描按键（需在循环中调用） */
void key_scan(void);

/* 注册按键回调函数 */
void key_register_callback(key_id_t id, void (*callback)(void));

#endif
