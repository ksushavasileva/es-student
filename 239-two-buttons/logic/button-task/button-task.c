#include "button-task/button-task.h"

#include <stddef.h>
#include <stdint.h>
#include "systime/systime.h"

// Время подавления дребезга: 20 мс.
#define BUTTON_DEBOUNCE_US 20000ULL
// Порог долгого нажатия: 1 с (без пауз и ожидания).
#define BUTTON_LONG_PRESS_US 1000000ULL

typedef enum
{
    BUTTON_STATE_RELEASED,
    BUTTON_STATE_PRESS_CHECK,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASE_CHECK
} button_state_t;

typedef struct
{
    button_state_t state;
    uint64_t change_us;
    uint64_t pressed_since_us;
    uint32_t press_count;
    bool long_reported;
    button_task_callback_t on_press;
    button_task_callback_t on_long_press;
} button_instance_t;

static button_instance_t buttons[PLATFORM_BUTTON_COUNT];

void button_task_init(platform_button_t button,
                      button_task_callback_t on_press,
                      button_task_callback_t on_long_press)
{
    if ((unsigned)button >= PLATFORM_BUTTON_COUNT)
    {
        return;
    }

    button_instance_t *item = &buttons[button];
    item->state = BUTTON_STATE_RELEASED;
    item->change_us = systime_us();
    item->pressed_since_us = 0;
    item->press_count = 0;
    item->long_reported = false;
    item->on_press = on_press;
    item->on_long_press = on_long_press;
}

void button_task_handle(void)
{
    uint64_t now_us = systime_us();

    // Для каждой кнопки — своя независимая машина состояний.
    for (unsigned id = 0; id < PLATFORM_BUTTON_COUNT; id++)
    {
        button_instance_t *button = &buttons[id];
        bool pressed = platform_button_read((platform_button_t)id);

        switch (button->state)
        {
            case BUTTON_STATE_RELEASED:
                if (pressed)
                {
                    button->change_us = now_us;
                    button->state = BUTTON_STATE_PRESS_CHECK;
                }
                break;

            case BUTTON_STATE_PRESS_CHECK:
                if (!pressed)
                {
                    button->state = BUTTON_STATE_RELEASED;
                }
                else if (now_us - button->change_us >= BUTTON_DEBOUNCE_US)
                {
                    button->state = BUTTON_STATE_PRESSED;
                    button->pressed_since_us = now_us;
                    button->long_reported = false;
                    button->press_count++;

                    // Для второй кнопки прежнее действие — при нажатии.
                    if (button->on_long_press == NULL && button->on_press != NULL)
                    {
                        button->on_press();
                    }
                }
                break;

            case BUTTON_STATE_PRESSED:
                if (!pressed)
                {
                    button->change_us = now_us;
                    button->state = BUTTON_STATE_RELEASE_CHECK;
                }
                else if (button->on_long_press != NULL &&
                         !button->long_reported &&
                         now_us - button->pressed_since_us >= BUTTON_LONG_PRESS_US)
                {
                    button->long_reported = true;
                    button->on_long_press();
                }
                break;

            case BUTTON_STATE_RELEASE_CHECK:
                if (pressed)
                {
                    // Ложное отпускание от дребезга: событие не повторяем.
                    button->state = BUTTON_STATE_PRESSED;
                }
                else if (now_us - button->change_us >= BUTTON_DEBOUNCE_US)
                {
                    button->state = BUTTON_STATE_RELEASED;

                    // Только короткое нажатие первой кнопки — при отпускании.
                    // После длинного события короткое уже не вызывается.
                    if (button->on_long_press != NULL &&
                        !button->long_reported &&
                        button->on_press != NULL)
                    {
                        button->on_press();
                    }
                }
                break;
        }
    }
}

bool button_task_is_pressed(platform_button_t button)
{
    if ((unsigned)button >= PLATFORM_BUTTON_COUNT)
    {
        return false;
    }

    return buttons[button].state == BUTTON_STATE_PRESSED ||
           buttons[button].state == BUTTON_STATE_RELEASE_CHECK;
}

uint32_t button_task_get_press_count(platform_button_t button)
{
    if ((unsigned)button >= PLATFORM_BUTTON_COUNT)
    {
        return 0;
    }

    return buttons[button].press_count;
}
