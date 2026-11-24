#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PI_STATE_NOT_STARTED,
    PI_STATE_RUNNING,
    PI_STATE_DONE
} pi_state_t;

void pi_task_init(void);
void pi_task_handle(void);

bool pi_task_start(uint32_t terms);
pi_state_t pi_task_get_state(void);
bool pi_task_get_result(double *value, uint64_t *elapsed_ms);
