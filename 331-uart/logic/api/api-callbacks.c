#include "api/api.h"
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
#include "edge-task/edge-task.h"
#include "uart-task/uart-task.h"
#include <ctype.h>
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

static void print_edge(platform_line_mode_t mode, bool ok, uint32_t rise_us, uint32_t fall_us)
{
    const char *name = mode == PLATFORM_LINE_PUSH_PULL ? "pp" : "od";
    if (!ok) { printf("edge %s: timeout\n", name); return; }
    printf("edge %s: rise %.1f ms, fall %.1f ms\n", name,
           rise_us / 1000.0, fall_us / 1000.0);
}

static void cmd_edge(const command_t *command)
{
    platform_line_mode_t mode;
    if (command->argc != 1) { printf("error: usage edge pp|od\n"); return; }
    if (strcmp(command->argv[0], "pp") == 0) mode = PLATFORM_LINE_PUSH_PULL;
    else if (strcmp(command->argv[0], "od") == 0) mode = PLATFORM_LINE_OPEN_DRAIN;
    else { printf("error: usage edge pp|od\n"); return; }
    if (!edge_task_start(mode, print_edge)) printf("error: edge is busy\n");
}

// UART0 loopback: hardware internal LBE or external GP0->GP1.
#define ECHO_MAX 32

static void print_bytes(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        putchar(isprint((unsigned char)data[i]) ? data[i] : '.');
    }
}

static void print_errors(uint32_t errors)
{
    printf("err:");
    if ((errors & 0xf) == 0)
    {
        printf(" none");
        return;
    }
    if (errors & PLATFORM_UART_FE) printf(" FE");
    if (errors & PLATFORM_UART_PE) printf(" PE");
    if (errors & PLATFORM_UART_BE) printf(" BE");
    if (errors & PLATFORM_UART_OE) printf(" OE");
}

static void cmd_loop(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage loop internal|pins\n");
        return;
    }
    if (strcmp(command->argv[0], "internal") == 0)
    {
        platform_uart_loop_set(true);
        printf("loop: internal\n");
    }
    else if (strcmp(command->argv[0], "pins") == 0)
    {
        platform_uart_loop_set(false);
        printf("loop: pins\n");
    }
    else
    {
        printf("error: usage loop internal|pins\n");
    }
}

static void print_echo(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("rx: ");
    print_bytes(data, size);
    putchar(' ');
    print_errors(errors);
    putchar('\n');
}

static void cmd_echo(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage echo <text>\n");
        return;
    }
    const char *value = command->argv[0];
    size_t len = strlen(value);
    if (len > ECHO_MAX)
    {
        printf("error: text longer than 32\n");
        return;
    }
    if (!uart_task_start(PLATFORM_UART0, PLATFORM_UART0,
                         (const uint8_t *)value, len, print_echo))
    {
        printf("error: uart is busy\n");
    }
}

// РўР°Р±Р»РёС†Р° РєРѕРјР°РЅРґ


// 3.3.3: two UARTs, two physical crossed wires.
static const char text_line[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

static void print_link_back(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("1->0: ");
    print_bytes(data, size);
    putchar(' ');
    print_errors(errors);
    putchar('\n');
}

static void print_link_forth(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("0->1: ");
    print_bytes(data, size);
    putchar(' ');
    print_errors(errors);
    putchar('\n');
    if (!uart_task_start(PLATFORM_UART1, PLATFORM_UART0, data, size, print_link_back))
    {
        printf("error: uart is busy\n");
    }
}

static void cmd_link(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage link <text>\n");
        return;
    }
    size_t length = strlen(command->argv[0]);
    if (length > ECHO_MAX)
    {
        printf("error: text longer than 32\n");
        return;
    }
    if (!uart_task_start(PLATFORM_UART0, PLATFORM_UART1,
                         (const uint8_t *)command->argv[0], length, print_link_forth))
    {
        printf("error: uart is busy\n");
    }
}

static void cmd_baud_shift(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage baud_shift <percent>\n");
        return;
    }
    const char *arg = command->argv[0];
    char *end = NULL;
    long percent = strtol(arg, &end, 10);
    if (!arg[0] || *end != '\0')
    {
        printf("error: usage baud_shift <percent>\n");
        return;
    }
    if (percent < -50 || percent > 50)
    {
        printf("error: percent must be -50..50\n");
        return;
    }
    uint32_t baud = (uint32_t)((115200L * (100 + percent)) / 100);
    uint32_t actual = platform_uart_baud_set(PLATFORM_UART1, baud);
    printf("baud_shift: uart1 %lu baud\n", (unsigned long)actual);
}

static void print_text(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("text: %lu chars ", (unsigned long)size);
    print_bytes(data, size);
    putchar(' ');
    print_errors(errors);
    putchar('\n');
}

static void cmd_text(const command_t *command)
{
    if (command->argc != 0)
    {
        printf("error: usage text\n");
        return;
    }
    if (!uart_task_start(PLATFORM_UART1, PLATFORM_UART0,
                         (const uint8_t *)text_line, strlen(text_line), print_text))
    {
        printf("error: uart is busy\n");
    }
}

static int parity_of(const char *value)
{
    if (strcmp(value, "none") == 0) return PLATFORM_PARITY_NONE;
    if (strcmp(value, "even") == 0) return PLATFORM_PARITY_EVEN;
    if (strcmp(value, "odd") == 0) return PLATFORM_PARITY_ODD;
    return -1;
}

static void cmd_parity(const command_t *command)
{
    if (command->argc != 2)
    {
        printf("error: usage parity none|even|odd none|even|odd\n");
        return;
    }
    int rx = parity_of(command->argv[0]);
    int tx = parity_of(command->argv[1]);
    if (rx < 0 || tx < 0)
    {
        printf("error: usage parity none|even|odd none|even|odd\n");
        return;
    }
    platform_uart_parity_set(PLATFORM_UART0, (platform_parity_t)rx);
    platform_uart_parity_set(PLATFORM_UART1, (platform_parity_t)tx);
    printf("parity: uart0 %s, uart1 %s\n", command->argv[0], command->argv[1]);
}

const api_command_t api_commands[] =
{
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
    {"button", "button state and press count", cmd_button},
    {"edge", "rise and fall of the RC line: edge pp|od", cmd_edge},
    {"loop", "UART0 loopback: loop internal|pins", cmd_loop},
    {"echo", "text through UART0 and back: echo <text>", cmd_echo},
    {"link", "text from UART0 to UART1 and back: link <text>", cmd_link},
    {"baud_shift", "shift UART1 baud rate: baud_shift <percent>", cmd_baud_shift},
    {"text", "32 characters from UART1 to UART0", cmd_text},
    {"parity", "parity of UART0 and UART1: parity <rx> <tx>", cmd_parity}};

const uint32_t api_command_count =
    sizeof(api_commands) / sizeof(api_commands[0]);

// РџРѕРёСЃРє РєРѕРјР°РЅРґС‹ Рё РІС‹РїРѕР»РЅРµРЅРёРµ РѕР±СЂР°Р±РѕС‚С‡РёРєР°
