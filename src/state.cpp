#include "state.hpp"
#include "device_info.hpp"
#include "board.hpp"
#include "boiler.hpp"
#include "protocol.hpp"
#include "crash_record.hpp"
#include "transport.hpp"
#include "diagnostic_storage.hpp"
#include "diagnostic_buffer.hpp"
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
using diagnostic_storage::Record;
using diagnostic_storage::Log;
using diagnostic_storage::Traffic;
static diagnostic_storage::Storage storage;
static Record *latest = nullptr;
static unsigned latest_count = 0;
static uint64_t latest_evicted = 0;
constexpr unsigned latest_capacity = 128;
static Network net;
static uint64_t retained = 0;
static uint64_t seq = 0, log_seq = 0, rx_bytes = 0, valid_count = 0, bad_count = 0, noise_count = 0;
static uint64_t overflow_count = 0, uart_errors = 0, evicted = 0, last_rx_ms = 0;
static uint64_t extended_until=0,next_capture_retry=0;
static unsigned speed = board::baud;
static uint64_t fifo_overflows=0,buffer_overflows=0,max_rx_gap_us=0,last_overflow_ms=0;
static size_t max_buffered=0,overflow_buffered=0;
void init() {
    mutex = xSemaphoreCreateMutex(); configASSERT(mutex);
    storage.init([](size_t count, size_t size, diagnostic_storage::Pool pool) {
        return heap_caps_calloc(count, size, MALLOC_CAP_8BIT |
            (pool == diagnostic_storage::Pool::external ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL));
    });
    latest = static_cast<Record *>(heap_caps_calloc(latest_capacity, sizeof(Record), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}
static bool resize_capture(bool enabled,uint64_t expected=0) {
    const size_t capacity=enabled?diagnostic_storage::Storage::extended_capacity:diagnostic_storage::Storage::history_capacity;
    {Lock lock;if(expected&&extended_until!=expected)return true;if(storage.capacity==capacity){extended_until=enabled?esp_timer_get_time()/1000+600000:0;return true;}}
    auto *next=static_cast<Record*>(heap_caps_calloc(capacity,sizeof(Record),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!next)return false;
    Record *previous=nullptr;
    {Lock lock;if(expected&&extended_until!=expected){heap_caps_free(next);return true;}
     const auto keep=diagnostic_storage::retain_latest(next,capacity,storage.records,storage.capacity,seq,retained);
     previous=storage.records;storage.records=next;storage.capacity=capacity;evicted+=retained-keep;retained=keep;extended_until=enabled?esp_timer_get_time()/1000+600000:0;}
    if(previous!=&storage.emergency_record)heap_caps_free(previous);
    return true;
}
bool extended_capture(bool enabled){return resize_capture(enabled);}
void maintenance(){
    const uint64_t ms=esp_timer_get_time()/1000;uint64_t expected=0;bool expired=false;
    {Lock lock;expired=extended_until&&ms>=extended_until&&ms>=next_capture_retry;if(expired){next_capture_retry=ms+10000;expected=extended_until;}}
    if(expired)resize_capture(false,expected);
}
void log(const char *format, ...) {
    char text[160];
    va_list ap; va_start(ap, format); vsnprintf(text, sizeof(text), format, ap); va_end(ap);
    { Lock lock; auto &entry = storage.entries[log_seq % storage.entry_capacity]; entry.seq = ++log_seq;
      entry.ms = esp_timer_get_time() / 1000; snprintf(entry.text, sizeof(entry.text), "%s", text); }
    printf("[ESP] %s\n", text);
}
void network(const Network &value) { Lock lock; net = value; }
Network network() { Lock lock; return net; }
static void append(const uint8_t *bytes, size_t length, bool checked) {
    Lock lock;
    auto &record = storage.records[seq % storage.capacity];
    if (retained == storage.capacity) ++evicted; else ++retained;
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
          if (storage.counters) {
              auto &type = storage.counters->types[data[7]], &device = storage.counters->devices[data[4]];
              ++type.count; type.ms = now; ++device.count; device.ms = now;
          }
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
void uart_overflow(bool fifo,size_t buffered){Lock lock;++uart_errors;++overflow_count;if(fifo)++fifo_overflows;else ++buffer_overflows;overflow_buffered=buffered;last_overflow_ms=esp_timer_get_time()/1000;}
void receiver_timing(uint64_t gap_us,size_t buffered){Lock lock;max_rx_gap_us=std::max(max_rx_gap_us,gap_us);max_buffered=std::max(max_buffered,buffered);}
void baud(unsigned value) { Lock lock; speed = value; }
cJSON *status() {
    auto *json = cJSON_CreateObject();
    if (!json) return nullptr;
    cJSON_AddStringToObject(json, "firmware", board::version);
    cJSON_AddItemToObject(json,"device",device_info::snapshot());
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
    {uint64_t fifo,buffer,gap,last;size_t peak,backlog;{Lock lock;fifo=fifo_overflows;buffer=buffer_overflows;gap=max_rx_gap_us;last=last_overflow_ms;peak=max_buffered;backlog=overflow_buffered;}
     auto *rx=cJSON_AddObjectToObject(json,"receiver");cJSON_AddNumberToObject(rx,"fifo_overflows",fifo);cJSON_AddNumberToObject(rx,"buffer_overflows",buffer);cJSON_AddNumberToObject(rx,"max_loop_gap_us",gap);cJSON_AddNumberToObject(rx,"max_buffered_bytes",peak);cJSON_AddNumberToObject(rx,"last_overflow_ms",last);cJSON_AddNumberToObject(rx,"last_overflow_buffered_bytes",backlog);}
    cJSON_AddNumberToObject(json, "baud", current_baud);
    {Lock lock;cJSON_AddNumberToObject(json,"capture_capacity",storage.capacity);cJSON_AddNumberToObject(json,"capture_extended_until_ms",extended_until);cJSON_AddBoolToObject(json,"capture_extended",storage.capacity>diagnostic_storage::Storage::history_capacity);}
    cJSON_AddNumberToObject(json, "log_capacity", storage.entry_capacity);
    cJSON_AddBoolToObject(json, "frame_stats_available", storage.counters != nullptr);
    cJSON_AddNumberToObject(json, "latest_frame_capacity", latest ? latest_capacity : 0);
    cJSON_AddNumberToObject(json, "max_frame_bytes", 1000);
    cJSON_AddItemToObject(json, "boiler", boiler::snapshot());
    auto *wifi = cJSON_AddObjectToObject(json, "wifi");
    cJSON_AddStringToObject(wifi, "ap_ssid", local.ap);
    cJSON_AddStringToObject(wifi, "ap_ip", "192.168.4.1");
    cJSON_AddStringToObject(wifi, "ssid", local.ssid);
    cJSON_AddStringToObject(wifi, "ip", local.ip);
    cJSON_AddStringToObject(wifi, "hostname", local.hostname);
    char local_url[80]; snprintf(local_url, sizeof(local_url), "http://%s.local/", local.hostname);
    cJSON_AddStringToObject(wifi, "local_url", local_url);
    cJSON_AddBoolToObject(wifi, "hostname_pending", local.hostname_pending);
    cJSON_AddNumberToObject(wifi, "hostname_error", local.hostname_error);
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
    DiagnosticBuffer<Record> copy; if(!copy.reserve(limit))return nullptr;
    uint64_t newest, oldest;
    { Lock lock; newest = seq; oldest = retained ? seq - retained + 1 : 0;
      for (uint64_t n = std::max(after + 1, oldest); n <= newest && copy.size() < limit; ++n)
          copy.push_back(storage.records[(n - 1) % storage.capacity]); }
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
        DiagnosticBuffer<char> hex(record.length * 3 + 1);
        if(!hex){cJSON_Delete(root);return nullptr;}
        for (size_t i = 0; i < record.length; ++i) {
            hex[i * 3] = digits[record.data[i] >> 4]; hex[i * 3 + 1] = digits[record.data[i] & 15]; hex[i * 3 + 2] = ' ';
        }
        hex.back() = 0; cJSON_AddStringToObject(item, "hex", hex.data());
    }
    return root;
}
cJSON *frame_stats() {
    DiagnosticBuffer<Traffic> type_copy(256), device_copy(256);
    if(!type_copy||!device_copy)return nullptr;
    { Lock lock; if (storage.counters) {
        memcpy(type_copy.data(), storage.counters->types, sizeof(storage.counters->types));
        memcpy(device_copy.data(), storage.counters->devices, sizeof(storage.counters->devices));
    } }
    auto *root = cJSON_CreateObject(); if (!root) return nullptr;
    cJSON_AddBoolToObject(root, "available", storage.counters != nullptr);
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
    DiagnosticBuffer<Record> copy; if(!copy.reserve(limit))return nullptr;
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
        DiagnosticBuffer<char> hex(record.length * 3 + 1);
        if(!hex){cJSON_Delete(root);return nullptr;}
        for (size_t i = 0; i < record.length; ++i) {
            hex[i * 3] = digits[record.data[i] >> 4]; hex[i * 3 + 1] = digits[record.data[i] & 15]; hex[i * 3 + 2] = ' ';
        }
        hex.back() = 0; cJSON_AddStringToObject(item, "hex", hex.data());
    }
    return root;
}
cJSON *logs(uint64_t after) {
    DiagnosticBuffer<Log> copy; if(!copy.reserve(storage.entry_capacity))return nullptr;
    { Lock lock; auto oldest = log_seq > storage.entry_capacity ? log_seq - storage.entry_capacity + 1 : (log_seq ? 1 : 0);
      for (auto n = std::max(after + 1, oldest); n <= log_seq; ++n) copy.push_back(storage.entries[(n - 1) % storage.entry_capacity]); }
    auto *array = cJSON_CreateArray();
    for (const auto &entry : copy) {
        auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(array, item);
        cJSON_AddNumberToObject(item, "seq", entry.seq); cJSON_AddNumberToObject(item, "ms", entry.ms);
        cJSON_AddStringToObject(item, "message", entry.text);
    }
    return array;
}
}

