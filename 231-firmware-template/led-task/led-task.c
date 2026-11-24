
#include "led-task.h"

#include "pico/stdlib.h"

#define LED_BLINK_PERIOD_MS 1000

static led_state_t led_state = LED_STATE_BLINK;
static uint32_t led_period_ms = LED_BLINK_PERIOD_MS;
static uint64_t last_toggle_us = 0;
static bool led_is_on = false;

// Проверяем, прошёл ли заданный интервал.
// Следующий момент отсчитываем от предыдущего,
// чтобы мигание не сдвигалось со временем.
static bool period_elapsed(uint64_t *last_us, uint64_t period_us)
{
    uint64_t now_us = time_us_64();

    if (now_us - *last_us < period_us)
    {
        return false;
    }

    // Если отстали больше чем на период,
    // не пытаемся догонять пропущенные переключения.
    if (now_us - *last_us >= 2 * period_us)
    {
        *last_us = now_us;
    }
    else
    {
        *last_us += period_us;
    }

    return true;
}

static void led_output_set(bool on)
{
    led_is_on = on;
    gpio_put(PICO_DEFAULT_LED_PIN, on);
}

void led_task_init(void)
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

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
            last_toggle_us = time_us_64();
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
    last_toggle_us = time_us_64();

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

            if (period_elapsed(&last_toggle_us, half_period_us))
            {
                led_output_set(!led_is_on);
            }

            break;
        }
    }
}
