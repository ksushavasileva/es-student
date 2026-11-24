#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "platform/platform.h"

// Задача сообщает о событиях, действия определяет main.c.
typedef void (*button_task_callback_t)(void);

// on_long_press == NULL: для кнопки действует прежняя схема,
// короткое событие сообщается после подавления дребезга нажатия.
void button_task_init(platform_button_t button,
                      button_task_callback_t on_press,
                      button_task_callback_t on_long_press);
void button_task_handle(void);
bool button_task_is_pressed(platform_button_t button);
uint32_t button_task_get_press_count(platform_button_t button);
