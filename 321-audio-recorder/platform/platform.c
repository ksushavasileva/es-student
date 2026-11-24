#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "platform/platform.h"
// Вывод заряда RC-цепи: GP22 (физическая ножка 29).
#define CHARGE_PIN 22

#include "hardware/adc.h"

// Вход АЦП: вывод GP26, канал 0.
#define ADC_PIN 26
#define ADC_CHANNEL 0
// Встроенный датчик температуры RP2040 подключён к каналу 4.
#define ADC_TEMP_CHANNEL 4

#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096


#include "pico/stdlib.h"
#include "pico/time.h"

#define LED_PIN PICO_DEFAULT_LED_PIN
#define BUTTON_PIN 15

void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, false);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    adc_init();
    adc_gpio_init(ADC_PIN);

    adc_set_temp_sensor_enabled(true);

    // При запуске конденсатор остаётся разряженным.
    gpio_init(CHARGE_PIN);
    gpio_put(CHARGE_PIN, false);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);
}

void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(void)
{
    return !gpio_get(BUTTON_PIN);
}

// Напряжение на GP26, В. Каждый вызов выбирает канал 0.
float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    uint32_t sum = 0;
    for (int i = 0; i < 32; ++i) {
        sum += adc_read();
    }
    return (sum / 32.0f) * ADC_VREF_V / ADC_LEVELS;
}

// Температура встроенного датчика, градусы Цельсия.
// Выбираем канал заново: после get_temp команда get_voltage читает GP26.
float platform_temp_read(void)
{
    adc_select_input(ADC_TEMP_CHANNEL);
    float voltage_v = adc_read() * ADC_VREF_V / ADC_LEVELS;
    return 27.0f - (voltage_v - 0.706f) / 0.001721f;
}

// Подаёт 3.3 В на резистор или заземляет его конец.
void platform_charge_set(bool on)
{
    gpio_put(CHARGE_PIN, on);
}

// Timer-based indicator, independent of led-task / main superloop.
static repeating_timer_t blink_timer;
static bool blink_running = false;
static volatile bool blink_on = false;
static volatile uint32_t blink_toggles = 0;

static bool on_blink(repeating_timer_t *timer)
{
    (void)timer;
    blink_on = !blink_on;
    gpio_put(LED_PIN, blink_on);
    blink_toggles++;
    return true;
}

bool platform_blink_start(uint32_t period_ms)
{
    if (period_ms < 10 || period_ms > 60000)
    {
        return false;
    }

    platform_blink_stop();
    blink_toggles = 0;
    blink_running = add_repeating_timer_ms(-(int32_t)period_ms, on_blink, NULL, &blink_timer);
    return blink_running;
}

uint32_t platform_blink_stop(void)
{
    if (blink_running)
    {
        cancel_repeating_timer(&blink_timer);
        blink_running = false;
    }
    blink_on = false;
    gpio_put(LED_PIN, 0);
    return blink_toggles;
}

// The timer callback only samples ADC; USB output is forbidden in IRQ context.
typedef enum { ADC_FREE, ADC_CAPTURE } adc_user_t;
static volatile adc_user_t adc_user = ADC_FREE;
static repeating_timer_t capture_timer;
static uint16_t capture_samples[PLATFORM_CAPTURE_MAX];
static uint32_t capture_times[PLATFORM_CAPTURE_MAX];
static uint32_t capture_count = 0;
static uint32_t capture_period_us = 0;
static volatile uint32_t capture_index = 0;
static volatile uint32_t capture_handler_us = 0;
static volatile bool capture_finished = false;

static bool on_capture(repeating_timer_t *timer)
{
    (void)timer;
    uint32_t start_us = time_us_32();
    uint32_t index = capture_index;
    if (index >= capture_count) {
        adc_user = ADC_FREE;
        capture_finished = true;
        return false;
    }
    capture_times[index] = start_us;
    capture_samples[index] = adc_read();
    capture_index = index + 1;
    capture_handler_us += time_us_32() - start_us;
    if (capture_index < capture_count) {
        return true;
    }
    adc_user = ADC_FREE;
    capture_finished = true;
    return false;
}

bool platform_adc_busy(void)
{
    return adc_user != ADC_FREE;
}

bool platform_capture_start(uint32_t count, uint32_t period_us)
{
    if (adc_user != ADC_FREE || count == 0 || count > PLATFORM_CAPTURE_MAX || period_us < 10) {
        return false;
    }
    // A new capture supersedes an already finished one; no timer remains active.
    adc_select_input(ADC_CHANNEL);
    capture_count = count;
    capture_period_us = period_us;
    capture_index = 0;
    capture_handler_us = 0;
    capture_finished = false;
    adc_user = ADC_CAPTURE;
    if (!add_repeating_timer_us(-(int64_t)period_us, on_capture, NULL, &capture_timer)) {
        adc_user = ADC_FREE;
        capture_count = 0;
        return false;
    }
    return true;
}

bool platform_capture_done(void)
{
    return capture_finished && capture_count != 0 && capture_index == capture_count;
}

void platform_capture_get(platform_capture_t *capture)
{
    if (capture == NULL) return;
    capture->samples = capture_samples;
    capture->count = capture_index;
    capture->period_us = capture_period_us;
    capture->min_interval_us = 0;
    capture->max_interval_us = 0;
    capture->handler_percent = 0.0f;

    if (!platform_capture_done() || capture_index < 2) return;
    uint32_t min_us = UINT32_MAX;
    uint32_t max_us = 0;
    for (uint32_t i = 1; i < capture_index; ++i) {
        uint32_t dt = capture_times[i] - capture_times[i - 1];
        if (dt < min_us) min_us = dt;
        if (dt > max_us) max_us = dt;
    }
    capture->min_interval_us = min_us;
    capture->max_interval_us = max_us;
    uint32_t elapsed_us = capture_times[capture_index - 1] - capture_times[0];
    if (elapsed_us != 0) {
        capture->handler_percent = 100.0f * (float)capture_handler_us / (float)elapsed_us;
    }
}
