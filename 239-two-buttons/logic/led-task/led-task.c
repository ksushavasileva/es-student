
#include "led-task/led-task.h"
#include "platform/platform.h"
#include "systime/systime.h"
#define LED_BLINK_PERIOD_MS 1000

static led_state_t led_state = LED_STATE_BLINK;
static uint32_t led_period_ms = LED_BLINK_PERIOD_MS;
static uint64_t last_toggle_us = 0;
static bool led_is_on = false;

// Проверяем, прошёл ли заданный интервал.
// Следующий момент отсчитываем от предыдущего,
// чтобы мигание не сдвигалось со временем.


static void led_output_set(bool on)
{
    led_is_on = on;
    platform_led_set(on);
}

void led_task_init(void)
{
    led_period_ms = LED_BLINK_PERIOD_MS;

    led_output_set(false);

    led_task_set_state(LED_STATE_BLINK);
}

void led_task_set_state(led_state_t state)
{
    led_state = state;

    switch (led_state)
    {
        case LED_STATE_OFF:
            led_output_set(false);
            break;

        case LED_STATE_ON:
            led_output_set(true);
            break;

        case LED_STATE_BLINK:
            // Первое переключение происходит сразу.
            led_output_set(!led_is_on);
            last_toggle_us = systime_us();
            break;
    }
}

led_state_t led_task_get_state(void)
{
    return led_state;
}

bool led_task_set_period_ms(uint32_t period_ms)
{
    if (period_ms == 0)
    {
        return false;
    }

    led_period_ms = period_ms;

    // Новую временную сетку начинаем с текущего момента.
    last_toggle_us = systime_us();

    return true;
}

uint32_t led_task_get_period_ms(void)
{
    return led_period_ms;
}

void led_task_handle(void)
{
    switch (led_state)
    {
        case LED_STATE_OFF:
            // Светодиод выключен.
            break;

        case LED_STATE_ON:
            // Светодиод включён.
            break;

        case LED_STATE_BLINK:
        {
            uint64_t half_period_us =
                (uint64_t)led_period_ms * 1000 / 2;

            if (systime_period_elapsed(&last_toggle_us, half_period_us))
            {
                led_output_set(!led_is_on);
            }

            break;
        }
    }
}

void led_task_next_state(void)
{
    switch (led_task_get_state())
    {
        case LED_STATE_OFF:
            led_task_set_state(LED_STATE_ON);
            break;

        case LED_STATE_ON:
            led_task_set_state(LED_STATE_BLINK);
            break;

        case LED_STATE_BLINK:
            led_task_set_state(LED_STATE_OFF);
            break;
    }
}

void led_task_next_period(void)
{
    uint32_t current = led_task_get_period_ms();
    uint32_t next;

    if (current == 250)
    {
        next = 500;
    }
    else if (current == 500)
    {
        next = 1000;
    }
    else
    {
        next = 250;
    }

    (void)led_task_set_period_ms(next);
}

void led_task_blink(void)
{
    led_task_set_state(LED_STATE_BLINK);
}
