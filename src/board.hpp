#pragma once
#include "driver/gpio.h"
#include "driver/uart.h"
#include "device_access.hpp"
namespace board {
constexpr gpio_num_t rx = GPIO_NUM_18;
constexpr gpio_num_t tx = GPIO_NUM_17;
constexpr gpio_num_t de = GPIO_NUM_21;
constexpr uart_port_t uart = UART_NUM_1;
constexpr unsigned baud = 115200;
constexpr const char *version = "0.7.0-beta.6";
constexpr const char *hostname = "piec";
constexpr bool github_ota = true;
inline const char *password() { return device_access::password(); }
constexpr const char *user = "admin";
}

