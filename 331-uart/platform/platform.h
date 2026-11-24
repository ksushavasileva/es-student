#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// USB standard I/O and hardware pins are configured here.
void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(void);

// GP22 line mode, 1k resistor to GP26 RC node.
typedef enum {
    PLATFORM_LINE_PUSH_PULL,
    PLATFORM_LINE_OPEN_DRAIN,
} platform_line_mode_t;
void platform_line_set(platform_line_mode_t mode, bool high);
float platform_voltage_read(void);

// UART0 TX=GP0, RX=GP1; UART1 is reserved for the next assignment.
typedef enum {
    PLATFORM_UART0,
    PLATFORM_UART1,
} platform_uart_t;

// Receive error bits in UARTRSR.
#define PLATFORM_UART_FE 0x1
#define PLATFORM_UART_PE 0x2
#define PLATFORM_UART_BE 0x4
#define PLATFORM_UART_OE 0x8

void platform_uart_loop_set(bool internal);
void platform_uart_clear(platform_uart_t uart);
bool platform_uart_put(platform_uart_t uart, uint8_t byte);
bool platform_uart_busy(platform_uart_t uart);
size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size);
uint32_t platform_uart_errors(platform_uart_t uart);

// UART format and speed configuration for the two-module experiment.
typedef enum {
    PLATFORM_PARITY_NONE,
    PLATFORM_PARITY_EVEN,
    PLATFORM_PARITY_ODD,
} platform_parity_t;

uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud);
void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity);
