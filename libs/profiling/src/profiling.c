#include "profiling/profiling.h"

#include <stddef.h>

#include "pico/stdlib.h"

typedef struct
{
    const char *name;
    uint32_t started_us;
    uint32_t sum_us;
    uint32_t count;
    uint32_t max_us;
    bool initialized;
} stopwatch_t;

static stopwatch_t stopwatches[PROFILING_STOPWATCH_COUNT];

void profiling_stopwatch_init(uint32_t id, const char *name)
{
    if (id >= PROFILING_STOPWATCH_COUNT)
    {
        return;
    }

    stopwatch_t *sw = &stopwatches[id];

    sw->name = name;
    sw->started_us = 0;
    sw->sum_us = 0;
    sw->count = 0;
    sw->max_us = 0;
    sw->initialized = true;
}

#if PROFILING_ENABLED
void profiling_start(uint32_t id)
{
    if (id >= PROFILING_STOPWATCH_COUNT ||
        !stopwatches[id].initialized)
    {
        return;
    }

    stopwatches[id].started_us = time_us_32();
}
#endif


#if PROFILING_ENABLED
void profiling_stop(uint32_t id)
{
    if (id >= PROFILING_STOPWATCH_COUNT ||
        !stopwatches[id].initialized)
    {
        return;
    }

    uint32_t now_us = time_us_32();
    stopwatch_t *sw = &stopwatches[id];

    uint32_t elapsed_us = now_us - sw->started_us;

    sw->sum_us += elapsed_us;
    sw->count++;

    if (elapsed_us > sw->max_us)
    {
        sw->max_us = elapsed_us;
    }
}
#endif


bool profiling_get(uint32_t id, profiling_result_t *result)
{
    if (id >= PROFILING_STOPWATCH_COUNT ||
        result == NULL ||
        !stopwatches[id].initialized)
    {
        return false;
    }

    const stopwatch_t *sw = &stopwatches[id];

    result->name = sw->name;
    result->count = sw->count;
    result->max_us = sw->max_us;

    result->mean_us = sw->count > 0
        ? (float)sw->sum_us / (float)sw->count
        : 0.0f;

    return true;
}

void profiling_reset(void)
{
    for (uint32_t i = 0; i < PROFILING_STOPWATCH_COUNT; i++)
    {
        stopwatches[i].started_us = 0;
        stopwatches[i].sum_us = 0;
        stopwatches[i].count = 0;
        stopwatches[i].max_us = 0;
    }
}
