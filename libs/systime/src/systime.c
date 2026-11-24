#include "systime/systime.h"

#include <stddef.h>
#include "pico/stdlib.h"

uint64_t systime_us(void)
{
    return time_us_64();
}

uint64_t systime_ms(void)
{
    return time_us_64() / 1000;
}

bool systime_period_elapsed(uint64_t *last_us, uint64_t period_us)
{
    if (last_us == NULL || period_us == 0)
    {
        return false;
    }

    uint64_t now_us = systime_us();
    uint64_t elapsed = now_us - *last_us;

    if (elapsed < period_us)
    {
        return false;
    }

    if (elapsed >= 2 * period_us)
    {
        *last_us = now_us;
    }
    else
    {
        *last_us += period_us;
    }

    return true;
}
