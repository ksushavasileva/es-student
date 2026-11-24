#include "pi-task.h"

#include "pico/stdlib.h"

// ???? ???? ???? ???????? ????? 585 ??????.
// ??? 125 ??? ??? ???????? 4.7 ???.
// 64 ????? ???????? ????? 301 ???, ?? ???? ?????? 0.5 ??.
#define PI_TERMS_PER_CALL 64

static pi_state_t pi_state = PI_STATE_NOT_STARTED;

static uint32_t pi_terms = 0;
static uint32_t pi_k = 0;

static double pi_sum = 0.0;
static double pi_sign = 1.0;
static double pi_value = 0.0;

static uint64_t pi_start_us = 0;
static uint64_t pi_elapsed_us = 0;

void pi_task_init(void)
{
    pi_state = PI_STATE_NOT_STARTED;
    pi_terms = 0;
    pi_k = 0;
    pi_sum = 0.0;
    pi_sign = 1.0;
    pi_value = 0.0;
    pi_start_us = 0;
    pi_elapsed_us = 0;
}

bool pi_task_start(uint32_t terms)
{
    if (pi_state == PI_STATE_RUNNING || terms == 0)
    {
        return false;
    }

    pi_terms = terms;
    pi_k = 0;
    pi_sum = 0.0;
    pi_sign = 1.0;
    pi_value = 0.0;
    pi_elapsed_us = 0;
    pi_start_us = time_us_64();

    pi_state = PI_STATE_RUNNING;
    return true;
}

void pi_task_handle(void)
{
    if (pi_state != PI_STATE_RUNNING)
    {
        return;
    }

    uint32_t end = pi_k + PI_TERMS_PER_CALL;

    // ?????? ?? ???????????? uint32_t.
    if (end < pi_k || end > pi_terms)
    {
        end = pi_terms;
    }

    while (pi_k < end)
    {
        pi_sum += pi_sign / (2.0 * (double)pi_k + 1.0);
        pi_sign = -pi_sign;
        pi_k++;
    }

    if (pi_k == pi_terms)
    {
        pi_value = 4.0 * pi_sum;
        pi_elapsed_us = time_us_64() - pi_start_us;
        pi_state = PI_STATE_DONE;
    }
}

pi_state_t pi_task_get_state(void)
{
    return pi_state;
}

bool pi_task_get_result(double *value, uint64_t *elapsed_ms)
{
    if (pi_state != PI_STATE_DONE ||
        value == NULL || elapsed_ms == NULL)
    {
        return false;
    }

    *value = pi_value;
    *elapsed_ms = pi_elapsed_us / 1000;
    return true;
}
