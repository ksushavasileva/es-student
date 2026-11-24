#include "edge-task/edge-task.h"
#include "systime/systime.h"

#define EMPTY_V 0.05f
#define RISE_V 2.0f
#define FULL_V 3.2f
#define FALL_V 1.3f
#define TIMEOUT_US 2000000ULL

typedef enum { EDGE_IDLE, EDGE_DISCHARGE, EDGE_RISE, EDGE_CHARGE, EDGE_FALL } edge_state_t;
static edge_state_t state;
static platform_line_mode_t current_mode;
static edge_task_callback_t callback;
static uint64_t phase_start;
static uint32_t rise_time;

static void enter(edge_state_t next, bool high)
{
    platform_line_set(current_mode, high);
    phase_start = systime_us();
    state = next;
}

static void finish(bool ok, uint32_t fall_time)
{
    edge_task_callback_t done = callback;
    platform_line_mode_t mode = current_mode;
    uint32_t rise = rise_time;
    state = EDGE_IDLE;
    callback = 0;
    platform_line_set(PLATFORM_LINE_PUSH_PULL, false);
    if (done) done(mode, ok, ok ? rise : 0, ok ? fall_time : 0);
}

bool edge_task_start(platform_line_mode_t mode, edge_task_callback_t on_done)
{
    if (state != EDGE_IDLE || on_done == 0) return false;
    current_mode = mode;
    callback = on_done;
    rise_time = 0;
    enter(EDGE_DISCHARGE, false);
    return true;
}

void edge_task_handle(void)
{
    if (state == EDGE_IDLE) return;
    uint64_t now = systime_us();
    if (now - phase_start > TIMEOUT_US) { finish(false, 0); return; }
    float volts = platform_voltage_read();
    switch (state)
    {
    case EDGE_DISCHARGE:
        if (volts < EMPTY_V) enter(EDGE_RISE, true);
        break;
    case EDGE_RISE:
        if (volts >= RISE_V) {
            rise_time = (uint32_t)(now - phase_start);
            // Leave line driven high until capacitor is nearly fully charged.
            phase_start = systime_us();
            state = EDGE_CHARGE;
        }
        break;
    case EDGE_CHARGE:
        if (volts >= FULL_V) enter(EDGE_FALL, false);
        break;
    case EDGE_FALL:
        if (volts <= FALL_V) finish(true, (uint32_t)(now - phase_start));
        break;
    default: break;
    }
}
