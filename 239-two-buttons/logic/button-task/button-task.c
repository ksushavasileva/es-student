#include "button-task/button-task.h"

#include <stddef.h>
#include <stdint.h>
#include "systime/systime.h"

// 20 мс подавления дребезга; ожидания и sleep нет.
#define BUTTON_DEBOUNCE_US 20000ULL

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
    uint32_t press_count;
    button_task_callback_t on_press;
} button_instance_t;

static button_instance_t buttons[PLATFORM_BUTTON_COUNT];

void button_task_init(platform_button_t button, button_task_callback_t on_press)
{
    if ((unsigned)button >= PLATFORM_BUTTON_COUNT)
    {
        return;
    }

    buttons[button].state = BUTTON_STATE_RELEASED;
    buttons[button].change_us = systime_us();
    buttons[button].press_count = 0;
    buttons[button].on_press = on_press;
}

void button_task_handle(void)
{
    uint64_t now_us = systime_us();

    // Каждая кнопка имеет независимый автомат состояний.
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
                    button->press_count++;
                    if (button->on_press != NULL)
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
                break;

            case BUTTON_STATE_RELEASE_CHECK:
                if (pressed)
                {
                    button->state = BUTTON_STATE_PRESSED;
                }
                else if (now_us - button->change_us >= BUTTON_DEBOUNCE_US)
                {
                    button->state = BUTTON_STATE_RELEASED;
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
