#pragma once
#include <cstdint>

namespace wifi_retry {
constexpr uint32_t delay(unsigned attempts) {
    unsigned shift=attempts?attempts-1:0;
    return shift>=4?15000U:1000U<<shift;
}
}
