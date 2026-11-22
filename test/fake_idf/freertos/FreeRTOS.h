#pragma once

#include <stdint.h>

using TickType_t = uint32_t;
#define configTICK_RATE_HZ 1000
constexpr uint32_t portTICK_PERIOD_MS = 1;
constexpr TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
