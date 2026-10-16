#pragma once
#include <cstddef>
inline void mbedtls_platform_zeroize(void* value, size_t length) {
    volatile unsigned char* bytes = static_cast<volatile unsigned char*>(value);
    while (length--) *bytes++ = 0;
}
