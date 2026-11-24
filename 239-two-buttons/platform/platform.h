#pragma once

#include <stdbool.h>

// Номера физических кнопок: задаются платформой, не логикой.
typedef enum
{
    PLATFORM_BUTTON_1,
    PLATFORM_BUTTON_2,
    PLATFORM_BUTTON_COUNT
} platform_button_t;

void platform_init(void);
void platform_led_set(bool on);

// Текущий электрический уровень, без программного подавления дребезга.
bool platform_button_read(platform_button_t button);
