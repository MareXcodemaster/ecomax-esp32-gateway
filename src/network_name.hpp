#pragma once
#include <cstddef>
#include <cstring>

namespace network_name {
inline bool valid(const char *name) {
    if (!name) return false;
    const size_t size = std::strlen(name);
    if (!size || size > 63 || name[0] == '-' || name[size - 1] == '-') return false;
    for (size_t i = 0; i < size; ++i)
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '-'))
            return false;
    return std::strcmp(name, "localhost") != 0;
}
}
