#include "systime/systime.h"

#include "button-task.h"

#include "pico/stdlib.h"
#include "led-task.h"

// Кнопка подключена между GP15 и GND
#define BUTTON_PIN 15

// Время подавления дребезга — 20 мс
#define BUTTON_DEBOUNCE_US 20000

typedef enum
{
    BUTTON_STATE_RELEASED,
    BUTTON_STATE_PRESS_CHECK,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASE_CHECK
} button_state_t;

static button_state_t button_state = BUTTON_STATE_RELEASED;

static uint64_t change_time_us = 0;
static uint32_t press_count = 0;

void button_task_init(void)
{
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    button_state = BUTTON_STATE_RELEASED;
    change_time_us = systime_us();
    press_count = 0;
}

void button_task_handle(void)
{
    // Кнопка замыкает GP15 на землю:
    // 0 — нажата, 1 — отпущена
    bool pressed = !gpio_get(BUTTON_PIN);

    uint64_t now_us = systime_us();

    switch (button_state)
    {
        case BUTTON_STATE_RELEASED:
            if (pressed)
            {
                change_time_us = now_us;
                button_state = BUTTON_STATE_PRESS_CHECK;
            }
            break;

        case BUTTON_STATE_PRESS_CHECK:
            if (!pressed)
            {
                button_state = BUTTON_STATE_RELEASED;
            }
            else if (now_us - change_time_us >= BUTTON_DEBOUNCE_US)
            {
                button_state = BUTTON_STATE_PRESSED;

                press_count++;

                led_task_next_state();
            }
            break;

        case BUTTON_STATE_PRESSED:
            if (!pressed)
            {
                change_time_us = now_us;
                button_state = BUTTON_STATE_RELEASE_CHECK;
            }
            break;

        case BUTTON_STATE_RELEASE_CHECK:
            if (pressed)
            {
                button_state = BUTTON_STATE_PRESSED;
            }
            else if (now_us - change_time_us >= BUTTON_DEBOUNCE_US)
            {
                button_state = BUTTON_STATE_RELEASED;
            }
            break;
    }
}

bool button_task_is_pressed(void)
{
    return button_state == BUTTON_STATE_PRESSED ||
           button_state == BUTTON_STATE_RELEASE_CHECK;
}

uint32_t button_task_get_press_count(void)
{
    return press_count;
}
