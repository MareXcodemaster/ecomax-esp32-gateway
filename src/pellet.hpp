#pragma once
#include "cJSON.h"
#include "sensor_decoder.hpp"
#include <string>

namespace pellet {
void init();
void receive(const plum::Sensors &sensor, uint64_t ms);
void checkpoint(bool force = false);
cJSON *snapshot();
bool configure(const cJSON *payload, std::string &error);
bool self_test();
}
