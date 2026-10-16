#pragma once
#include "cJSON.h"
#include <cstdint>

namespace panel_history {
void start();
void clear();
void append(const cJSON *curr, bool native, double epoch, uint64_t monotonic_ms);
cJSON *snapshot(uint32_t from=0,uint32_t to=0);
}
