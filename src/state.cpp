#include "state.hpp"
#include "board.hpp"
#include "boiler.hpp"
#include "protocol.hpp"
#include "crash_record.hpp"
#include "transport.hpp"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

namespace state {
static SemaphoreHandle_t mutex;
struct Lock {
    Lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex); }
};
struct Record { uint64_t seq, ms; uint16_t length; bool validated; uint8_t data[1000]; };
struct Log { uint64_t seq, ms; char text[160]; };
static Record fallback_records[48]{};
static Record *records = fallback_records;
static size_t capacity = 48;
static Record *latest = nullptr;
static unsigned latest_count = 0;
static uint64_t latest_evicted = 0;
constexpr unsigned latest_capacity = 128;
struct Traffic { uint64_t count = 0, ms = 0; };
static Traffic types[256], devices[256];
static Log entries[48]{};
static Network net;
static uint64_t seq = 0, log_seq = 0, rx_bytes = 0, valid_count = 0, bad_count = 0, noise_count = 0;
static uint64_t overflow_count = 0, uart_errors = 0, evicted = 0, last_rx_ms = 0;
static unsigned speed = board::baud;
void init() {
    mutex = xSemaphoreCreateMutex(); configASSERT(mutex);
    auto *history = static_cast<Record *>(heap_caps_calloc(2048, sizeof(Record), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (history) { records = history; capacity = 2048; }
    latest = static_cast<Record *>(heap_caps_calloc(latest_capacity, sizeof(Record), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}
void log(const char *format, ...) {
    char text[160];
    va_list ap; va_start(ap, format); vsnprintf(text, sizeof(text), format, ap); va_end(ap);
    { Lock lock; auto &entry = entries[log_seq % 48]; entry.seq = ++log_seq;
      entry.ms = esp_timer_get_time() / 1000; snprintf(entry.text, sizeof(entry.text), "%s", text); }
    printf("[LAZAR] %s\n", text);
}
void network(const Network &value) { Lock lock; net = value; }
Network network() { Lock lock; return net; }
static void append(const uint8_t *bytes, size_t length, bool checked) {
    Lock lock;
    auto &record = records[seq % capacity];
    if (seq >= capacity) ++evicted;
    record.seq = ++seq; record.ms = esp_timer_get_time() / 1000;
    record.length = std::min(length, sizeof(record.data)); record.validated = checked;
    memcpy(record.data, bytes, record.length);
    if (!checked) { rx_bytes += length; last_rx_ms = record.ms; }
}
void raw(const uint8_t *data, size_t length) { append(data, length, false); }
void frame(const uint8_t *data, size_t length) {
    append(data, length, true);
    if (length >= 10) {
        { Lock lock; auto now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
          ++types[data[7]].count; types[data[7]].ms = now;
          ++devices[data[4]].count; devices[data[4]].ms = now;
          if (latest) {
              unsigned index = latest_count;
              for (unsigned i = 0; i < latest_count; ++i)
                  if (latest[i].data[4] == data[4] && latest[i].data[7] == data[7]) { index = i; break; }
              if (index == latest_count) {
                  if (latest_count < latest_capacity) ++latest_count;
                  else {
                      index = 0;
                      for (unsigned i = 1; i < latest_count; ++i) if (latest[i].ms < latest[index].ms) index = i;
                      ++latest_evicted;
                  }
              }
              auto &record = latest[index]; record.seq = seq; record.ms = now; record.validated = true;
              record.length = std::min(length, sizeof(record.data)); memcpy(record.data, data, record.length);
          } }
        boiler::receive(data, length);
        protocol::receive(data, length);
    }
}
void parser_stats(uint64_t valid, uint64_t bad, uint64_t noise) {
    Lock lock; valid_count = valid; bad_count = bad; noise_count = noise;
}
void uart_error(bool overflow) { Lock lock; ++uart_errors; if (overflow) ++overflow_count; }
void baud(unsigned value) { Lock lock; speed = value; }
cJSON *status() {
    auto *json = cJSON_CreateObject();
    if (!json) return nullptr;
    cJSON_AddStringToObject(json, "firmware", board::version);
    cJSON_AddStringToObject(json, "mode", transport::enabled()?"RS485_DIRECT":"PASSIVE_RX_ONLY");
    cJSON_AddBoolToObject(json, "tx_enabled", transport::enabled());
    cJSON_AddBoolToObject(json, "rs485_address_conflict", transport::conflict());
    cJSON_AddNumberToObject(json, "tx_bytes", transport::sent_bytes());
    cJSON_AddNumberToObject(json, "de_level", gpio_get_level(board::de));
    cJSON_AddNumberToObject(json, "uptime_ms", esp_timer_get_time() / 1000);
    cJSON_AddNumberToObject(json, "reset_reason", esp_reset_reason());
    cJSON_AddItemToObject(json,"last_crash",crash_record::snapshot());
    cJSON_AddNumberToObject(json, "heap_free", esp_get_free_heap_size());
    cJSON_AddNumberToObject(json, "internal_heap_free", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(json, "internal_heap_largest", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(json, "caller_stack_free_bytes", uxTaskGetStackHighWaterMark(nullptr));
    cJSON_AddNumberToObject(json, "heap_min", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(json, "psram_bytes", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    char sha[65];
    const auto *description = esp_app_get_description();
    for (unsigned i = 0; i < 32; ++i) snprintf(sha + i * 2, 3, "%02x", description->app_elf_sha256[i]);
    cJSON_AddStringToObject(json, "app_elf_sha256", sha);
    Network local; uint64_t values[9]; unsigned current_baud;
    { Lock lock; local = net; current_baud = speed;
      uint64_t v[] = {rx_bytes, valid_count, bad_count, noise_count, overflow_count, uart_errors, evicted, last_rx_ms, seq};
      memcpy(values, v, sizeof(values)); }
    const char *keys[] = {"rx_bytes", "valid_frames", "invalid_candidates", "noise_bytes", "uart_overflows",
                         "uart_errors", "capture_evicted", "last_rx_ms", "capture_seq"};
    for (int i = 0; i < 9; ++i) cJSON_AddNumberToObject(json, keys[i], values[i]);
    cJSON_AddNumberToObject(json, "baud", current_baud);
    cJSON_AddNumberToObject(json, "capture_capacity", capacity);
    cJSON_AddNumberToObject(json, "latest_frame_capacity", latest ? latest_capacity : 0);
    cJSON_AddNumberToObject(json, "max_frame_bytes", 1000);
    cJSON_AddItemToObject(json, "boiler", boiler::snapshot());
    auto *wifi = cJSON_AddObjectToObject(json, "wifi");
    cJSON_AddStringToObject(wifi, "ap_ssid", local.ap);
    cJSON_AddStringToObject(wifi, "ap_ip", "192.168.4.1");
    cJSON_AddStringToObject(wifi, "ssid", local.ssid);
    cJSON_AddStringToObject(wifi, "ip", local.ip);
    cJSON_AddStringToObject(wifi, "hostname", board::hostname);
    cJSON_AddStringToObject(wifi, "local_url", "http://piec.local/");
    cJSON_AddBoolToObject(wifi, "fixed", local.fixed);
    cJSON_AddBoolToObject(wifi, "mdns_ready", local.mdns);
    cJSON_AddBoolToObject(wifi, "connected", local.connected);
    cJSON_AddNumberToObject(wifi, "rssi", local.rssi);
    cJSON_AddNumberToObject(wifi, "channel", local.channel);
    cJSON_AddNumberToObject(wifi, "disconnect_reason", local.reason);
    cJSON_AddNumberToObject(wifi, "retries", local.retries);
    return json;
}
cJSON *capture(uint64_t after, unsigned limit) {
    limit = std::min(limit, 32U);
    std::vector<Record> copy; copy.reserve(limit);
    uint64_t newest, oldest;
    { Lock lock; newest = seq; oldest = seq > capacity ? seq - capacity + 1 : (seq ? 1 : 0);
      for (uint64_t n = std::max(after + 1, oldest); n <= newest && copy.size() < limit; ++n)
          copy.push_back(records[(n - 1) % capacity]); }
    auto *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "oldest_seq", oldest); cJSON_AddNumberToObject(root, "latest_seq", newest);
    cJSON_AddBoolToObject(root, "gap", oldest > 1 && after < oldest - 1);
    cJSON_AddNumberToObject(root, "next_after", copy.empty() ? after : copy.back().seq);
    auto *array = cJSON_AddArrayToObject(root, "records");
    const char digits[] = "0123456789ABCDEF";
    for (const auto &record : copy) {
        auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(array, item);
        cJSON_AddNumberToObject(item, "seq", record.seq); cJSON_AddNumberToObject(item, "ms", record.ms);
        cJSON_AddNumberToObject(item, "length", record.length); cJSON_AddBoolToObject(item, "validated", record.validated);
        if (record.validated) {
            cJSON_AddNumberToObject(item, "destination", record.data[3]);
            cJSON_AddNumberToObject(item, "source", record.data[4]);
            cJSON_AddNumberToObject(item, "type", record.data[7]);
        }
        std::vector<char> hex(record.length * 3 + 1);
        for (size_t i = 0; i < record.length; ++i) {
            hex[i * 3] = digits[record.data[i] >> 4]; hex[i * 3 + 1] = digits[record.data[i] & 15]; hex[i * 3 + 2] = ' ';
        }
        hex.back() = 0; cJSON_AddStringToObject(item, "hex", hex.data());
    }
    return root;
}
cJSON *frame_stats() {
    std::vector<Traffic> type_copy(256), device_copy(256);
    { Lock lock; memcpy(type_copy.data(), types, sizeof(types)); memcpy(device_copy.data(), devices, sizeof(devices)); }
    auto *root = cJSON_CreateObject(); if (!root) return nullptr;
    auto now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    auto *frames = cJSON_AddArrayToObject(root, "types");
    auto *nodes = cJSON_AddArrayToObject(root, "devices");
    for (unsigned i = 0; i < 256; ++i) {
        if (type_copy[i].count) {
            auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(frames, item);
            cJSON_AddNumberToObject(item, "type", i); cJSON_AddNumberToObject(item, "count", type_copy[i].count);
            cJSON_AddNumberToObject(item, "age_ms", now - type_copy[i].ms);
            protocol::add_stats(item, i);
        }
        if (device_copy[i].count) {
            auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(nodes, item);
            cJSON_AddNumberToObject(item, "address", i); cJSON_AddNumberToObject(item, "count", device_copy[i].count);
            cJSON_AddNumberToObject(item, "age_ms", now - device_copy[i].ms);
        }
    }
    return root;
}
cJSON *latest_frames(int source, int type, unsigned offset, unsigned limit, bool decode) {
    limit = std::min(limit, 16U);
    std::vector<Record> copy; copy.reserve(limit);
    uint64_t evictions; unsigned total = 0;
    { Lock lock; evictions = latest_evicted;
      if (latest) for (unsigned i = 0; i < latest_count; ++i) {
          if ((source >= 0 && latest[i].data[4] != source) || (type >= 0 && latest[i].data[7] != type)) continue;
          if (total >= offset && copy.size() < limit) copy.push_back(latest[i]);
          ++total;
      } }
    auto *root = cJSON_CreateObject(); if (!root) return nullptr;
    cJSON_AddBoolToObject(root, "available", latest != nullptr);
    cJSON_AddNumberToObject(root, "total", total); cJSON_AddNumberToObject(root, "evicted_keys", evictions);
    cJSON_AddNumberToObject(root, "next_offset", offset + copy.size());
    auto *array = cJSON_AddArrayToObject(root, "records");
    const char digits[] = "0123456789ABCDEF";
    for (const auto &record : copy) {
        auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(array, item);
        cJSON_AddNumberToObject(item, "seq", record.seq); cJSON_AddNumberToObject(item, "ms", record.ms);
        cJSON_AddNumberToObject(item, "source", record.data[4]); cJSON_AddNumberToObject(item, "destination", record.data[3]);
        cJSON_AddNumberToObject(item, "type", record.data[7]); cJSON_AddNumberToObject(item, "length", record.length);
        cJSON_AddBoolToObject(item, "validated", true);
        if (decode) cJSON_AddItemToObject(item, "decoded", protocol::describe(record.data, record.length));
        std::vector<char> hex(record.length * 3 + 1);
        for (size_t i = 0; i < record.length; ++i) {
            hex[i * 3] = digits[record.data[i] >> 4]; hex[i * 3 + 1] = digits[record.data[i] & 15]; hex[i * 3 + 2] = ' ';
        }
        hex.back() = 0; cJSON_AddStringToObject(item, "hex", hex.data());
    }
    return root;
}
cJSON *logs(uint64_t after) {
    std::vector<Log> copy; copy.reserve(48);
    { Lock lock; auto oldest = log_seq > 48 ? log_seq - 47 : (log_seq ? 1 : 0);
      for (auto n = std::max(after + 1, oldest); n <= log_seq; ++n) copy.push_back(entries[(n - 1) % 48]); }
    auto *array = cJSON_CreateArray();
    for (const auto &entry : copy) {
        auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(array, item);
        cJSON_AddNumberToObject(item, "seq", entry.seq); cJSON_AddNumberToObject(item, "ms", entry.ms);
        cJSON_AddStringToObject(item, "message", entry.text);
    }
    return array;
}
}

