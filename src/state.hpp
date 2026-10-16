#pragma once
#include <cstdint>
#include <cstddef>
#include "cJSON.h"
namespace state {
struct Network {
    char ap[33]{}, ssid[33]{}, ip[16]{};
    bool connected = false;
    bool fixed = false, mdns = false;
    int rssi = 0, channel = 0, reason = 0;
    unsigned retries = 0;
};
void init();
void log(const char *format, ...) __attribute__((format(printf, 1, 2)));
void network(const Network &net);
Network network();
void raw(const uint8_t *data, size_t size);
void frame(const uint8_t *data, size_t size);
void parser_stats(uint64_t valid, uint64_t invalid, uint64_t noise);
void uart_error(bool overflow);
void baud(unsigned value);
cJSON *status();
cJSON *capture(uint64_t after, unsigned limit);
cJSON *logs(uint64_t after);
cJSON *frame_stats();
cJSON *latest_frames(int source, int type, unsigned offset, unsigned limit, bool decode = false);
}

