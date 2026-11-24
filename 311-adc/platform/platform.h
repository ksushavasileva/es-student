#pragma once

#include <stdbool.h>

// USB standard I/O and hardware pins are configured here.
void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(void);

// Напряжение на входе GP26 в вольтах.
float platform_voltage_read(void);
