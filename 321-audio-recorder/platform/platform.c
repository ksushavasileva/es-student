#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "platform/platform.h"
// Вывод заряда RC-цепи: GP22 (физическая ножка 29).
#define CHARGE_PIN 22

#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/stdio.h"

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

// One-block DMA capture. DMA completion is the only per-block IRQ.
typedef enum { ADC_FREE, ADC_CAPTURE, ADC_BLOCK, ADC_STREAM } adc_user_t;
static volatile adc_user_t adc_user = ADC_FREE;

#define ADC_CLOCK_HZ 48000000.0f
static int block_channel = -1;
static uint16_t block_samples[PLATFORM_BLOCK_MAX];
static uint32_t block_start_us;
static volatile uint32_t block_end_us;
static volatile bool block_ready;

static void adc_free_running(void)
{
    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(ADC_CLOCK_HZ / PLATFORM_SAMPLE_RATE_HZ - 1);
}

static void adc_one_shot(void)
{
    adc_run(false);
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    adc_set_clkdiv(0);
}

static void on_block_done(void)
{
    dma_channel_acknowledge_irq1(block_channel);
    block_end_us = time_us_32();
    adc_one_shot();
    block_ready = true;
    adc_user = ADC_FREE;
}

bool platform_block_start(uint32_t count)
{
    if (adc_user != ADC_FREE || count == 0 || count > PLATFORM_BLOCK_MAX)
        return false;

    adc_free_running();
    dma_channel_config cfg = dma_channel_get_default_config(block_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    block_ready = false;
    adc_user = ADC_BLOCK;
    dma_channel_configure(block_channel, &cfg, block_samples, &adc_hw->fifo, count, true);
    block_start_us = time_us_32();
    adc_run(true);
    return true;
}

bool platform_block_done(void)
{
    return block_ready;
}

const uint16_t *platform_block_samples(void)
{
    return block_samples;
}

uint32_t platform_block_time_us(void)
{
    return block_end_us - block_start_us;
}

// Two chained DMA channels: IRQ 0 only signals a completed buffer.
// Frame formatting and USB writes happen in stream-task/API outside the IRQ.
static int stream_channels[2] = {-1, -1};
static uint8_t stream_buffers[2][PLATFORM_STREAM_BYTES] __attribute__((aligned(4)));
static volatile bool stream_ready[2];
static volatile uint32_t stream_overruns;
static uint32_t stream_samples;
static uint32_t stream_next;

static void on_stream_buffer(void)
{
    for (int i = 0; i < 2; ++i) {
        if (!dma_channel_get_irq0_status(stream_channels[i])) continue;
        dma_channel_acknowledge_irq0(stream_channels[i]);
        // Re-arm the completed channel's write pointer for its next turn.
        dma_channel_set_write_addr(stream_channels[i], stream_buffers[i], false);
        if (stream_ready[(i + 1) % 2]) stream_overruns++;
        stream_ready[i] = true;
    }
}

bool platform_stream_start(uint8_t bits)
{
    if (adc_user != ADC_FREE || bits != 16) return false;
    stream_samples = PLATFORM_STREAM_BYTES / 2;
    adc_free_running();
    for (int i = 0; i < 2; ++i) {
        int next_channel = stream_channels[(i + 1) % 2];
        dma_channel_config cfg = dma_channel_get_default_config(stream_channels[i]);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, DREQ_ADC);
        channel_config_set_chain_to(&cfg, next_channel);
        dma_channel_configure(stream_channels[i], &cfg, stream_buffers[i],
                              &adc_hw->fifo, stream_samples, false);
        dma_channel_set_irq0_enabled(stream_channels[i], true);
        stream_ready[i] = false;
    }
    stream_overruns = 0;
    stream_next = 0;
    adc_user = ADC_STREAM;
    dma_channel_start(stream_channels[0]);
    adc_run(true);
    return true;
}

void platform_stream_stop(void)
{
    if (adc_user != ADC_STREAM) return;
    adc_run(false);
    // RP2040-E13: break DMA chaining BEFORE aborting either channel.
    for (int i = 0; i < 2; ++i) {
        dma_channel_config cfg = dma_get_channel_config(stream_channels[i]);
        channel_config_set_chain_to(&cfg, stream_channels[i]);
        dma_channel_set_config(stream_channels[i], &cfg, false);
    }
    for (int i = 0; i < 2; ++i) {
        dma_channel_set_irq0_enabled(stream_channels[i], false);
        dma_channel_abort(stream_channels[i]);
        dma_channel_acknowledge_irq0(stream_channels[i]);
    }
    adc_one_shot();
    adc_user = ADC_FREE;
}

const uint8_t *platform_stream_next(uint32_t *samples)
{
    if (adc_user != ADC_STREAM || !stream_ready[stream_next]) return NULL;
    if (samples != NULL) *samples = stream_samples;
    return stream_buffers[stream_next];
}

void platform_stream_release(void)
{
    stream_ready[stream_next] = false;
    stream_next = (stream_next + 1) % 2;
}

uint32_t platform_stream_overruns(void)
{
    return stream_overruns;
}

void platform_write(const void *data, uint32_t bytes)
{
    // Raw USB bytes: no LF -> CRLF conversion, no suffix newline.
    stdio_put_string((const char *)data, (int)bytes, false, false);
}

// Audio test tone is generated entirely by PWM hardware on GP20.
#define TONE_PIN 20
#define TONE_COUNT_HZ 1000000u

bool platform_tone_set(uint32_t freq_hz)
{
    if (freq_hz == 0) {
        pwm_set_gpio_level(TONE_PIN, 0);
        return true;
    }
    if (freq_hz < 20 || freq_hz > 20000) return false;

    // 1 MHz counter: TOP+1 is the tone period in microseconds.
    uint32_t period_us = TONE_COUNT_HZ / freq_hz;
    uint slice = pwm_gpio_to_slice_num(TONE_PIN);
    pwm_set_wrap(slice, (uint16_t)(period_us - 1));
    pwm_set_gpio_level(TONE_PIN, (uint16_t)(period_us / 2));
    return true;
}

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

    // One PWM tick = one microsecond. Idle output is always low.
    gpio_set_function(TONE_PIN, GPIO_FUNC_PWM);
    uint tone_slice = pwm_gpio_to_slice_num(TONE_PIN);
    pwm_config tone_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&tone_cfg, (float)clock_get_hz(clk_sys) / TONE_COUNT_HZ);
    pwm_config_set_wrap(&tone_cfg, 999);
    pwm_init(tone_slice, &tone_cfg, true);
    pwm_set_gpio_level(TONE_PIN, 0);

    // IRQ 1 is dedicated to completed ADC DMA blocks.
    block_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq1_enabled(block_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, on_block_done);
    irq_set_enabled(DMA_IRQ_1, true);

    stream_channels[0] = dma_claim_unused_channel(true);
    stream_channels[1] = dma_claim_unused_channel(true);
    irq_set_exclusive_handler(DMA_IRQ_0, on_stream_buffer);
    irq_set_enabled(DMA_IRQ_0, true);
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
