#include "api/api.h"
#include "telemetry-task/telemetry-task.h"
#include "platform/platform.h"
#include "api/api-commands.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device/device.h"
#include "profiling/profiling.h"
#include "systime/systime.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "firmware.h"

static bool parse_u32(const char *text, uint32_t *value)
{
    if (text == NULL || *text == '\0' || *text == '-')
    {
        return false;
    }

    errno = 0;
    char *end = NULL;

    unsigned long number = strtoul(text, &end, 10);

    if (errno == ERANGE ||
        end == text ||
        *end != '\0' ||
        number > UINT32_MAX)
    {
        return false;
    }

    *value = (uint32_t)number;
    return true;
}

// РџР°СЃРїРѕСЂС‚ РїСЂРёР±РѕСЂР°
static void cmd_info(const command_t *command)
{
    (void)command;

    device_info_t info;
    device_get_info(&info);

    printf("name: %s\n", FIRMWARE_NAME);
    printf("version: %s\n", FIRMWARE_VERSION);
    printf("project: %s\n", FIRMWARE_PROJECT);
    printf("repo: %s\n", FIRMWARE_REPO);
    printf("board: %s\n", info.board);
    printf("serial: %s\n", info.serial);
    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %u\n",
           (unsigned)info.manufacturer,
           (unsigned)info.part,
           (unsigned)info.revision);
    printf("pico-sdk: %s\n", info.sdk_version);
}

// Р’СЂРµРјСЏ СЃ РјРѕРјРµРЅС‚Р° РІРєР»СЋС‡РµРЅРёСЏ РїР»Р°С‚С‹
static void cmd_uptime(const command_t *command)
{
    (void)command;

    printf("uptime: %llu ms\n",
           (unsigned long long)(systime_ms()));
}

// Р’С‹С‡РёСЃР»РµРЅРёРµ С‡РёСЃР»Р° РїРё
// ????????? ????????????? ?????? ????? pi.
static void cmd_pi_start(const command_t *command)
{
    uint32_t terms = 1000000;

    if (command->argc > 1 ||
        (command->argc == 1 &&
         !parse_u32(command->argv[0], &terms)))
    {
        printf("error: usage pi_start [terms]\n");
        return;
    }

    if (terms == 0)
    {
        printf("error: terms must be greater than 0\n");
        return;
    }

    if (!pi_task_start(terms))
    {
        printf("error: pi is busy\n");
        return;
    }

    printf("pi: started, %u terms\n", (unsigned)terms);
}

// ???????? ????????? ??? ????????? ???????.
static void cmd_pi(const command_t *command)
{
    (void)command;

    pi_state_t state = pi_task_get_state();

    if (state == PI_STATE_NOT_STARTED)
    {
        printf("pi: not started\n");
        return;
    }

    if (state == PI_STATE_RUNNING)
    {
        printf("pi: running\n");
        return;
    }

    double value = 0.0;
    uint64_t elapsed_ms = 0;

    if (pi_task_get_result(&value, &elapsed_ms))
    {
        printf("pi: %.9f in %llu ms\n",
               value,
               (unsigned long long)elapsed_ms);
    }
}

static void cmd_profiling(const command_t *command)
{
#if !PROFILING_ENABLED
    (void)command;
    printf("profiling is disabled in this build\n");
    return;
#endif

    (void)command;

    profiling_result_t result;

    for (uint32_t id = 0;
         id < PROFILING_STOPWATCH_COUNT;
         id++)
    {
        if (profiling_get(id, &result))
        {
            printf("%-10s count %-10u mean %.2f us  max %u us\n",
                   result.name,
                   (unsigned)result.count,
                   (double)result.mean_us,
                   (unsigned)result.max_us);
        }
    }
}

static void cmd_profiling_reset(const command_t *command)
{
    (void)command;

    profiling_reset();
    printf("profiling reset\n");
}

// РЎР±СЂРѕСЃ РјР°РєСЃРёРјР°Р»СЊРЅРѕРіРѕ РІСЂРµРјРµРЅРё РёС‚РµСЂР°С†РёРё


// РћР±С‰РёР№ РѕС‚РІРµС‚ Рѕ СЃРѕСЃС‚РѕСЏРЅРёРё СЃРІРµС‚РѕРґРёРѕРґР°
static void print_led_status(void)
{
    const char *state_name = "off";

    switch (led_task_get_state())
    {
        case LED_STATE_OFF:
            state_name = "off";
            break;

        case LED_STATE_ON:
            state_name = "on";
            break;

        case LED_STATE_BLINK:
            state_name = "blink";
            break;
    }

    printf("led: %s, period %u ms\n",
           state_name,
           (unsigned)led_task_get_period_ms());
}

// Р’РєР»СЋС‡РёС‚СЊ СЃРІРµС‚РѕРґРёРѕРґ
static void cmd_led_enable(const command_t *command)
{
    (void)command;

    led_task_set_state(LED_STATE_ON);
    print_led_status();
}

// Р’С‹РєР»СЋС‡РёС‚СЊ СЃРІРµС‚РѕРґРёРѕРґ
static void cmd_led_disable(const command_t *command)
{
    (void)command;

    led_task_set_state(LED_STATE_OFF);
    print_led_status();
}

// Р’РєР»СЋС‡РёС‚СЊ РјРёРіР°РЅРёРµ
static void cmd_led_blink(const command_t *command)
{
    (void)command;

    led_task_set_state(LED_STATE_BLINK);
    print_led_status();
}

// РР·РјРµРЅРёС‚СЊ РїРµСЂРёРѕРґ РјРёРіР°РЅРёСЏ
static void cmd_led_period(const command_t *command)
{
    uint32_t period_ms = 0;

    if (command->argc != 1 ||
        !parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage led_period <period_ms>\n");
        return;
    }

    if (!led_task_set_period_ms(period_ms))
    {
        printf("error: period_ms must be greater than 0\n");
        return;
    }

    print_led_status();
}

static void cmd_button(const command_t *command)
{
    (void)command;

    const char *state =
        button_task_is_pressed() ? "pressed" : "released";

    printf("button: %s, presses %u\n",
           state,
           (unsigned)button_task_get_press_count());
}

// РўР°Р±Р»РёС†Р° РєРѕРјР°РЅРґ

// Измеряем напряжение через platform; SDK в логике не используется.
static void cmd_get_voltage(const command_t *command)
{
    (void)command;
    printf("voltage: %.3f V\n", (double)platform_voltage_read());
}

// Температура кристалла RP2040, измерение через platform.
static void cmd_get_temp(const command_t *command)
{
    (void)command;
    printf("temp: %.1f C\n", (double)platform_temp_read());
}

// Period of unsolicited telemetry by default: 100 ms.
#define TELEMETRY_DEFAULT_PERIOD_MS 100

void api_print_telemetry(const telemetry_record_t *record)
{
    if (record == NULL)
    {
        return;
    }

    printf("tm: %llu %.3f %.1f\n",
           (unsigned long long)record->time_us,
           (double)record->voltage_v,
           (double)record->temp_c);
}

static void cmd_tm_start(const command_t *command)
{
    uint32_t period_ms = TELEMETRY_DEFAULT_PERIOD_MS;

    if (command->argc > 1 ||
        (command->argc == 1 &&
         !parse_u32(command->argv[0], &period_ms)))
    {
        printf("error: usage tm_start [<period_ms>]\n");
        return;
    }

    if (!telemetry_task_start(period_ms))
    {
        printf("error: period_ms must be at least 10\n");
        return;
    }

    printf("telemetry: started, period %u ms\n", (unsigned)period_ms);
}

static void cmd_tm_stop(const command_t *command)
{
    (void)command;
    telemetry_task_stop();
    printf("telemetry: stopped\n");
}

// Управление зарядом RC-цепи через platform.
static void cmd_charge_on(const command_t *command)
{
    (void)command;
    platform_charge_set(true);
    printf("charge: on\n");
}

static void cmd_charge_off(const command_t *command)
{
    (void)command;
    platform_charge_set(false);
    printf("charge: off\n");
}




const api_command_t api_commands[] =
{
    { "charge_on", "start charging RC circuit", cmd_charge_on },
    { "charge_off", "stop charging RC circuit", cmd_charge_off },
    { "tm_start", "start telemetry: tm_start [<period_ms>]", cmd_tm_start },
    { "tm_stop", "stop telemetry", cmd_tm_stop },
    { "get_temp", "chip temperature, degrees Celsius", cmd_get_temp },
    { "get_voltage", "voltage on GP26, ADC input 0", cmd_get_voltage },
    {"info", "device passport", cmd_info},
    {"uptime", "time since reset", cmd_uptime},
    {"pi_start", "start pi calculation: pi_start [terms]", cmd_pi_start},
    {"pi", "pi calculation result", cmd_pi},
    {"profiling", "stopwatch statistics", cmd_profiling},
    {"profiling_reset", "reset stopwatches", cmd_profiling_reset},

    {"led_enable", "turn LED on", cmd_led_enable},
    {"led_disable", "turn LED off", cmd_led_disable},
    {"led_blink", "make LED blink", cmd_led_blink},
    
    {"led_period", "set blinking period: led_period <period_ms>", cmd_led_period},
    {"button", "button state and press count", cmd_button}

};

const uint32_t api_command_count =
    sizeof(api_commands) / sizeof(api_commands[0]);

// РџРѕРёСЃРє РєРѕРјР°РЅРґС‹ Рё РІС‹РїРѕР»РЅРµРЅРёРµ РѕР±СЂР°Р±РѕС‚С‡РёРєР°

