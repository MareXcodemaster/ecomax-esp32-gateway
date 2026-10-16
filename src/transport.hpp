#pragma once
#include "esp_err.h"
#include <cstddef>
#include <cstdint>
#include "cJSON.h"
namespace transport {
void lock_tx();
void start();
bool set_baud(unsigned value);
bool self_test();
struct Reply { uint8_t bytes[1000]{}; size_t length=0; bool sent=false; unsigned latency_ms=0; };
bool enable();
void disable();
bool enabled();
bool conflict();
uint64_t sent_bytes();
bool request(uint8_t type, const uint8_t *payload, size_t size, uint8_t response, Reply &reply);
bool request_readonly(uint8_t type, const uint8_t *payload, size_t size, uint8_t response, Reply &reply);
bool request_thermostat(uint8_t type, const uint8_t *payload, size_t size, uint8_t response, Reply &reply);
bool read_thermostat_schedule(Reply &reply);
}

