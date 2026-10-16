#pragma once
#include <cstddef>
#include <cstdint>
#include "cJSON.h"
namespace protocol {
void init();
void receive(const uint8_t *data, size_t length);
void add_stats(cJSON *item, unsigned type);
cJSON *describe(const uint8_t *data, size_t length);
bool self_test();
}
