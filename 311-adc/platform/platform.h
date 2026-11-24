#pragma once

#include <stdbool.h>

// USB standard I/O and hardware pins are configured here.
void platform_init(void);
void platform_led_set(bool on);
bool platform_button_read(void);

// Напряжение на входе GP26 в вольтах.
float platform_voltage_read(void);

// Температура кристалла RP2040 по встроенному датчику, градусы Цельсия.
float platform_temp_read(void);

// Включить / выключить заряд RC-цепи через GP22.
void platform_charge_set(bool on);
