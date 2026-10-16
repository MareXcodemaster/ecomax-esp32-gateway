#pragma once
#include "history_data.hpp"
#include "cJSON.h"

namespace history_store {
void start(uint32_t schema_signature);
bool enqueue(const history_data::Sample &sample);
size_t read(uint32_t from,uint32_t to,history_data::Point *points,size_t capacity);
cJSON *status();
}
