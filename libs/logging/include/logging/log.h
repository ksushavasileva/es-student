#pragma once

#include <stdio.h>

#define LOG_DBG(...) do { printf("[DBG] "); printf(__VA_ARGS__); } while (0)
#define LOG_INF(...) do { printf("[INF] "); printf(__VA_ARGS__); } while (0)
#define LOG_ERR(...) do { printf("[ERR] "); printf(__VA_ARGS__); } while (0)
