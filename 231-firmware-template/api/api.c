#include "led-task.h"
#include "api.h"
#include "button-task.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include "device.h"
#include "profiling.h"

typedef void (*api_callback_t)(const command_t *command);

typedef struct
{
    const char *name;
    const char *help;
    api_callback_t callback;
} api_command_t;

// Разбор целого числа из аргумента команды
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

// Паспорт прибора
static void cmd_info(const command_t *command)
{
    (void)command;

    device_info();
}

// Время с момента включения платы
static void cmd_uptime(const command_t *command)
{
    (void)command;

    printf("uptime: %llu ms\n",
           (unsigned long long)(time_us_64() / 1000));
}

// Вычисление числа пи
static double calc_pi(uint32_t terms)
{
    double sum = 0.0;
    double sign = 1.0;

    for (uint32_t k = 0; k < terms; k++)
    {
        sum += sign / (2.0 * (double)k + 1.0);
        sign = -sign;
    }

    return 4.0 * sum;
}

static volatile double pi_result;

static void cmd_calc_pi(const command_t *command)
{
    uint32_t terms = 1000000;

    if (command->argc > 1 ||
        (command->argc == 1 &&
         !parse_u32(command->argv[0], &terms)))
    {
        printf("error: usage calc_pi [terms]\n");
        return;
    }

    if (terms == 0)
    {
        printf("error: terms must be greater than 0\n");
        return;
    }

    uint64_t start_us = time_us_64();

    pi_result = calc_pi(terms);

    uint64_t spent_us = time_us_64() - start_us;

    printf("pi: %.9f\n", (double)pi_result);

    printf("time: %llu ms\n",
           (unsigned long long)(spent_us / 1000));
}

// Среднее и максимальное время итерации
static void cmd_main_time_exec(const command_t *command)
{
    (void)command;

    printf("iteration avg %.2f us, max %u us\n",
           profiling_avg_us(),
           (unsigned)profiling_max_us());
}

// Сброс максимального времени итерации
static void cmd_main_time_reset(const command_t *command)
{
    (void)command;

    profiling_reset_max();

    printf("max reset\n");
}

// Общий ответ о состоянии светодиода
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

// Включить светодиод
static void cmd_led_enable(const command_t *command)
{
    (void)command;

    led_task_set_state(LED_STATE_ON);
    print_led_status();
}

// Выключить светодиод
static void cmd_led_disable(const command_t *command)
{
    (void)command;

    led_task_set_state(LED_STATE_OFF);
    print_led_status();
}

// Включить мигание
static void cmd_led_blink(const command_t *command)
{
    (void)command;

    led_task_set_state(LED_STATE_BLINK);
    print_led_status();
}

// Изменить период мигания
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

// Таблица команд

static const api_command_t api_commands[] =
{
    {"info", "device passport", cmd_info},
    {"uptime", "time since reset", cmd_uptime},
    {"calc_pi", "calculate pi: calc_pi [terms]", cmd_calc_pi},
    {"main_time_exec", "superloop iteration: average and maximum", cmd_main_time_exec},
    {"main_time_reset", "reset maximum iteration", cmd_main_time_reset},

    {"led_enable", "turn LED on", cmd_led_enable},
    {"led_disable", "turn LED off", cmd_led_disable},
    {"led_blink", "make LED blink", cmd_led_blink},
    
    {"led_period", "set blinking period: led_period <period_ms>", cmd_led_period},
    {"button", "button state and press count", cmd_button}

};

static const uint32_t api_command_count =
    sizeof(api_commands) / sizeof(api_commands[0]);

// Поиск команды и выполнение обработчика
void api_handle(const command_t *command)
{
    if (command == NULL)
    {
        return;
    }

    if (command->truncated)
    {
        printf("error: command longer than 63 characters or 4 arguments\n");
        return;
    }

    if (command->name == NULL)
    {
        return;
    }

    if (strcmp(command->name, "help") == 0)
    {
        printf("%-16s %s\n", "help", "list of commands");

        for (uint32_t i = 0; i < api_command_count; i++)
        {
            printf("%-16s %s\n",
                   api_commands[i].name,
                   api_commands[i].help);
        }

        return;
    }

    for (uint32_t i = 0; i < api_command_count; i++)
    {
        if (strcmp(command->name, api_commands[i].name) == 0)
        {
            if (api_commands[i].callback != NULL)
            {
                api_commands[i].callback(command);
            }

            return;
        }
    }

    printf("error: unknown command '%s', try help\n",
           command->name);
}
