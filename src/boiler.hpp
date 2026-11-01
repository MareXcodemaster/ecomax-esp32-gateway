#pragma once
#include <cstdint>
#include <cstddef>
#include "cJSON.h"
namespace boiler {
void init();
void receive(const uint8_t *frame, size_t size);
cJSON *snapshot();
bool water_heater_temperature(double &value,uint64_t &age_ms);
bool self_test();
}
