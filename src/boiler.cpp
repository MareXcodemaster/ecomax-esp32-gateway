#include "boiler.hpp"
#include "sensor_decoder.hpp"
#include "sensor_fixtures.hpp"
#include "pellet.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cmath>
#include <cstring>

namespace boiler {
static SemaphoreHandle_t mutex;
static plum::Sensors current;
static uint64_t accepted = 0, rejected = 0, updated_ms = 0;
static bool available = false;
constexpr uint64_t stale_ms = 15000;
void init() { mutex = xSemaphoreCreateMutex(); configASSERT(mutex); }
void receive(const uint8_t *frame, size_t size) {
    if (size < 10 || frame[4] != 0x45 || frame[7] != 53) return;
    plum::Sensors next;
    bool ok = plum::decode_sensors(frame + 8, size - 10, next);
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (ok) { current = next; available = true; ++accepted; updated_ms = esp_timer_get_time() / 1000; }
    else ++rejected;
    xSemaphoreGive(mutex);
    if (ok) pellet::receive(next, esp_timer_get_time() / 1000);
}
static void number(cJSON *root, const char *key, double value) {
    if (std::isfinite(value)) cJSON_AddNumberToObject(root, key, value);
    else cJSON_AddNullToObject(root, key);
}
static void optional(cJSON *root, const char *key, int value) {
    if (value >= 0) cJSON_AddNumberToObject(root, key, value); else cJSON_AddNullToObject(root, key);
}
cJSON *snapshot() {
    plum::Sensors s; uint64_t count, bad, ms; bool have;
    xSemaphoreTake(mutex, portMAX_DELAY);
    s = current; count = accepted; bad = rejected; ms = updated_ms; have = available;
    xSemaphoreGive(mutex);
    auto *root = cJSON_CreateObject(); if (!root) return nullptr;
    uint64_t now = esp_timer_get_time() / 1000;
    cJSON_AddBoolToObject(root, "available", have);
    cJSON_AddBoolToObject(root, "fresh", have && now - ms <= stale_ms);
    if (have) cJSON_AddNumberToObject(root, "age_ms", now - ms); else cJSON_AddNullToObject(root, "age_ms");
    cJSON_AddNumberToObject(root, "stale_after_ms", stale_ms);
    cJSON_AddNumberToObject(root, "updated_ms", ms);
    cJSON_AddNumberToObject(root, "accepted_frames", count); cJSON_AddNumberToObject(root, "decode_errors", bad);
    cJSON_AddStringToObject(root, "source", "RS485_SENSOR_DATA_53");
    cJSON_AddNumberToObject(root, "address", 0x45); cJSON_AddBoolToObject(root, "write_enabled", false);
    if (!have) return root;
    auto *sensors = cJSON_AddObjectToObject(root, "sensors");
    for (unsigned i = 0; i < 17; ++i) number(sensors, plum::temperatures[i], s.temperature[i]);
    optional(sensors, "heating_target", s.heating_target); optional(sensors, "water_heater_target", s.water_heater_target);
    optional(sensors, "heating_status", s.heating_status); optional(sensors, "water_heater_status", s.water_heater_status);
    cJSON_AddNumberToObject(sensors, "state", s.state); cJSON_AddNumberToObject(sensors, "transmission", s.transmission);
    number(sensors, "fan_power", s.fan_power); number(sensors, "boiler_power", s.boiler_power);
    number(sensors, "fuel_consumption", s.fuel_consumption); optional(sensors, "fuel_level", s.fuel_level);
    optional(sensors,"fuel_level_raw",s.fuel_level_raw);
    if(s.fuel_calibrating>=0)cJSON_AddBoolToObject(sensors,"fuel_calibrating",s.fuel_calibrating==1);else cJSON_AddNullToObject(sensors,"fuel_calibrating");
    optional(sensors, "boiler_load", s.boiler_load); optional(sensors, "lambda_state", s.lambda_state);
    number(sensors, "lambda_level", s.lambda_level); optional(sensors, "lambda_target", s.lambda_target);
    cJSON_AddNumberToObject(sensors, "pending_alerts", s.pending_alerts);
    auto *out = cJSON_AddObjectToObject(root, "outputs");
    for (unsigned i = 0; i < 16; ++i) cJSON_AddBoolToObject(out, plum::outputs[i], s.output_bits & (1U << i));
    cJSON_AddNumberToObject(root, "output_flags", s.output_flags);
    auto *versions = cJSON_AddObjectToObject(root, "modules");
    for (unsigned i = 0; i < 6; ++i) {
        if (*s.module_version[i]) cJSON_AddStringToObject(versions, plum::modules[i], s.module_version[i]);
        else cJSON_AddNullToObject(versions, plum::modules[i]);
    }
    auto *thermostats = cJSON_AddArrayToObject(root, "thermostats");
    for (unsigned i = 0; i < s.thermostat_slots; ++i) {
        const auto &t = s.thermostats[i]; if (!t.available) continue;
        auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(thermostats, item);
        cJSON_AddNumberToObject(item, "index", i); cJSON_AddNumberToObject(item, "state", t.state);
        number(item, "current_temp", t.current); number(item, "target_temp", t.target);
        cJSON_AddBoolToObject(item, "contacts", t.contact); cJSON_AddBoolToObject(item, "schedule", t.schedule);
    }
    auto *mixers = cJSON_AddArrayToObject(root, "mixers");
    for (unsigned i = 0; i < s.mixer_slots; ++i) {
        const auto &m = s.mixers[i]; if (!m.available) continue;
        auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(mixers, item);
        cJSON_AddNumberToObject(item, "index", i); number(item, "current_temp", m.current);
        optional(item, "target_temp", m.target); cJSON_AddBoolToObject(item, "pump", m.pump);
    }
    cJSON_AddNumberToObject(root, "thermostat_slots", s.thermostat_slots);
    cJSON_AddNumberToObject(root, "mixer_slots", s.mixer_slots);
    auto *frame_versions = cJSON_AddObjectToObject(root, "frame_versions");
    for (unsigned i = 0; i < 256; ++i) if (s.version_present[i]) {
        char key[4]; std::snprintf(key, sizeof(key), "%u", i); cJSON_AddNumberToObject(frame_versions, key, s.versions[i]);
    }
    return root;
}
bool self_test() {
    unsigned passed = 0;
    for (const auto &fixture : sensor_fixtures) {
        plum::Sensors s;
        if (!plum::decode_sensors(fixture.data, fixture.size, s) ||
            std::fabs(s.temperature[0] - fixture.heating) > 0.00001f ||
            std::fabs(s.temperature[2] - fixture.water) > 0.00001f ||
            s.heating_target != 75 || s.water_heater_target != 44 ||
            std::strcmp(s.module_version[0], "8.41.38.L0") ||
            std::strcmp(s.module_version[5], "8.30.28") ||
            s.thermostat_slots != 3 || s.mixer_slots != 4 ||
            !s.thermostats[0].available || !s.mixers[0].available) return false;
        for (size_t n = 0; n < fixture.size; ++n) if (plum::decode_sensors(fixture.data, n, s)) return false;
        ++passed;
    }
    return passed == sensor_fixture_count;
}
}
