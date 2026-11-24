#pragma once

#include <stdbool.h>
#include <stdint.h>

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

// Timer-driven recording indicator; no dependency on superloop timing.
// Period is the interval between LED toggles (10..60000 ms).
bool platform_blink_start(uint32_t period_ms);
// Stop indicator, turn LED off and return toggles since last start.
uint32_t platform_blink_stop(void);
