#pragma once

#include <stdbool.h>

// USB standard I/O and hardware pins are configured here.
void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(void);
