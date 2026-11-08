#pragma once
#include "sampling_policy.hpp"
#include <string>
namespace sampling {
void init();
uint32_t boiler_ms();
uint32_t panel_ms();
uint32_t history_ms();
cJSON *snapshot();
bool configure(const cJSON *body,std::string &error);
}
