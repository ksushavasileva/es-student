
#pragma once

#include "telemetry-task/telemetry-task.h"

#include "stdio-text-protocol/stdio-text-protocol.h"

void api_handle(const command_t *command);

// Print one telemetry record as a text-protocol line.
void api_print_telemetry(const telemetry_record_t *record);
