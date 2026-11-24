#pragma once

#include <stdbool.h>
#include <stdint.h>

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
