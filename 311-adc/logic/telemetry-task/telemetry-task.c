#include "telemetry-task/telemetry-task.h"

#include <stddef.h>

#include "platform/platform.h"
#include "systime/systime.h"

// State of non-blocking periodic telemetry task.
static telemetry_state_t telemetry_state = TELEMETRY_STATE_IDLE;
static telemetry_task_callback_t telemetry_on_record = NULL;
static uint32_t telemetry_period_ms = 100;
static uint64_t telemetry_last_us = 0;
static bool telemetry_first_record = false;

void telemetry_task_init(telemetry_task_callback_t on_record)
{
    telemetry_on_record = on_record;
    telemetry_state = TELEMETRY_STATE_IDLE;
    telemetry_period_ms = 100;
    telemetry_last_us = 0;
    telemetry_first_record = false;
}

bool telemetry_task_start(uint32_t period_ms)
{
    if (period_ms < 10)
    {
        return false;
    }

    telemetry_period_ms = period_ms;
    telemetry_last_us = systime_us();
    telemetry_first_record = true;
    telemetry_state = TELEMETRY_STATE_RUN;
    return true;
}

void telemetry_task_stop(void)
{
    telemetry_state = TELEMETRY_STATE_IDLE;
    telemetry_first_record = false;
}

telemetry_state_t telemetry_task_get_state(void)
{
    return telemetry_state;
}

uint32_t telemetry_task_get_period_ms(void)
{
    return telemetry_period_ms;
}

void telemetry_task_handle(void)
{
    switch (telemetry_state)
    {
        case TELEMETRY_STATE_IDLE:
            break;

        case TELEMETRY_STATE_RUN:
        {
            bool due = false;
            if (telemetry_first_record)
            {
                telemetry_first_record = false;
                due = true;
            }
            else
            {
                due = systime_period_elapsed(
                    &telemetry_last_us,
                    (uint64_t)telemetry_period_ms * 1000u);
            }

            if (due)
            {
                telemetry_record_t record;
                record.time_us = systime_us();
                record.voltage_v = platform_voltage_read();
                record.temp_c = platform_temp_read();

                if (telemetry_on_record != NULL)
                {
                    telemetry_on_record(&record);
                }
            }
            break;
        }
    }
}
