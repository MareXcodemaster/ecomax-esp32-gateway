#include "pellet.hpp"
#include "fuel_meter.hpp"
#include "state.hpp"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cstring>

namespace pellet {
namespace {
constexpr uint32_t magic = 0x50454c31;
constexpr uint64_t checkpoint_ms = 600000;
struct Store {
    uint32_t signature = magic, revision = 0;
    double sample_kg = 0, sample_seconds = 0;
    Totals totals;
};
SemaphoreHandle_t mutex, storage_mutex;
Store config;
Meter meter;
uint64_t updated_ms = 0, checkpoint_at = 0;
double reported_flow = NAN, flow = NAN;
Source source = Source::unavailable;
int level = -1;
int raw_level=-1,calibrating=-1;
bool have_sensor = false, feeder_available = false, feeder = false;
bool dirty = false, storage_ok = true, restored = false;
struct Lock {
    SemaphoreHandle_t handle;
    explicit Lock(SemaphoreHandle_t h) : handle(h) { xSemaphoreTake(h, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(handle); }
};
uint64_t now() { return esp_timer_get_time() / 1000; }
bool save(const Store &value) {
    nvs_handle_t handle;
    if (nvs_open("pellet", NVS_READWRITE, &handle) != ESP_OK) return false;
    auto result = nvs_set_blob(handle, "meter_v1", &value, sizeof(value));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle); return result == ESP_OK;
}
bool valid_store(const Store &value) {
    const auto &t = value.totals;
    return value.signature == magic &&
        ((value.sample_kg == 0 && value.sample_seconds == 0) || std::isfinite(calibration_rate(value.sample_kg, value.sample_seconds))) &&
        std::isfinite(t.reported_kg) && t.reported_kg >= 0 && t.reported_kg <= 1e9 &&
        std::isfinite(t.estimated_kg) && t.estimated_kg >= 0 && t.estimated_kg <= 1e9 &&
        t.counted_ms <= 3155760000000ULL && t.omitted_ms <= 3155760000000ULL &&
        t.reported_ms <= t.counted_ms && t.estimated_ms <= t.counted_ms && t.reported_ms + t.estimated_ms == t.counted_ms;
}
void number(cJSON *object, const char *key, double value) {
    if (std::isfinite(value)) cJSON_AddNumberToObject(object, key, value);
    else cJSON_AddNullToObject(object, key);
}
}
void init() {
    mutex = xSemaphoreCreateMutex(); storage_mutex = xSemaphoreCreateMutex();
    configASSERT(mutex && storage_mutex);
    nvs_handle_t handle;
    auto result = nvs_open("pellet", NVS_READONLY, &handle);
    if (result == ESP_OK) {
        Store loaded; size_t size = sizeof(loaded);
        result = nvs_get_blob(handle, "meter_v1", &loaded, &size); nvs_close(handle);
        if (result == ESP_OK && size == sizeof(loaded) && valid_store(loaded)) {
            config = loaded; meter.totals = loaded.totals; restored = true;
        } else if (result != ESP_ERR_NVS_NOT_FOUND) storage_ok = false;
    } else if (result != ESP_ERR_NVS_NOT_FOUND) storage_ok = false;
    checkpoint_at = now();
}
void receive(const plum::Sensors &sensor, uint64_t ms) {
    Lock lock(mutex);
    have_sensor = true; updated_ms = ms; level = sensor.fuel_level;
    raw_level=sensor.fuel_level_raw;calibrating=sensor.fuel_calibrating;
    reported_flow = valid_rate(sensor.fuel_consumption) ? sensor.fuel_consumption : NAN;
    feeder_available = sensor.output_flags & 2U; feeder = sensor.output_bits & 2U;
    auto calibrated = calibration_rate(config.sample_kg, config.sample_seconds);
    if (std::isfinite(reported_flow)) { flow = reported_flow; source = Source::reported; }
    else if (feeder_available && std::isfinite(calibrated)) { flow = feeder ? calibrated : 0; source = Source::feeder_estimate; }
    else { flow = NAN; source = Source::unavailable; }
    auto before = meter.totals;
    meter.sample(ms, flow, source);
    if (std::memcmp(&before, &meter.totals, sizeof(before))) dirty = true;
}
void checkpoint(bool force) {
    Lock storage(storage_mutex);
    Store value;
    {
        Lock lock(mutex);
        if (!dirty || (!force && now() - checkpoint_at < checkpoint_ms)) return;
        value = config; value.totals = meter.totals; dirty = false; checkpoint_at = now();
    }
    bool ok = save(value);
    { Lock lock(mutex); storage_ok = ok; if (!ok) dirty = true; }
}
cJSON *snapshot() {
    Store copy; Meter counter; double actual, current; Source origin; int percent,raw,calibration_state;
    uint64_t ms; bool have, on, present, stored, resumed;
    {
        Lock lock(mutex); copy = config; counter = meter; actual = reported_flow; current = flow; origin = source;
        percent = level; ms = updated_ms; have = have_sensor; on = feeder; present = feeder_available;
        raw=raw_level;calibration_state=calibrating;
        stored = storage_ok; resumed = restored;
    }
    auto *root = cJSON_CreateObject(); if (!root) return nullptr;
    bool fresh = have && now() - ms <= 5000;
    cJSON_AddBoolToObject(root, "fresh", fresh);
    number(root, "age_seconds", have ? (now() - ms) / 1000.0 : NAN);
    number(root, "level_percent", fresh && percent >= 0 && percent <= 100 ? percent : NAN);
    number(root,"level_raw",fresh&&raw>=0?raw:NAN);
    if(fresh&&calibration_state>=0)cJSON_AddBoolToObject(root,"level_calibrating",calibration_state==1);else cJSON_AddNullToObject(root,"level_calibrating");
    cJSON_AddStringToObject(root, "level_source", "Poziom z kotla / nie pomiar zasobnika");
    number(root, "reported_flow_kg_h", fresh ? actual : NAN);
    number(root, "flow_kg_h", fresh ? current : NAN);
    cJSON_AddStringToObject(root, "flow_source", !fresh || origin == Source::unavailable ? "unavailable" : origin == Source::reported ? "boiler" : "feeder_estimate");
    if (fresh && present) cJSON_AddBoolToObject(root, "feeder_on", on); else cJSON_AddNullToObject(root, "feeder_on");
    number(root, "consumed_kg", counter.totals.counted_ms ? counter.totals.reported_kg + counter.totals.estimated_kg : NAN);
    number(root, "reported_consumed_kg", counter.totals.reported_ms ? counter.totals.reported_kg : NAN);
    number(root, "estimated_consumed_kg", counter.totals.estimated_ms ? counter.totals.estimated_kg : NAN);
    number(root, "counted_seconds", counter.totals.counted_ms / 1000.0);
    number(root, "omitted_seconds", counter.totals.omitted_ms / 1000.0);
    cJSON_AddBoolToObject(root, "restored_after_restart", resumed);
    cJSON_AddBoolToObject(root, "storage_ok", stored);
    cJSON_AddNumberToObject(root, "checkpoint_seconds", checkpoint_ms / 1000);
    cJSON_AddBoolToObject(root, "boiler_calibration_supported", false);
    cJSON_AddStringToObject(root, "boiler_calibration_status", "Brak potwierdzonej komendy kalibracji zasobnika RS485");
    auto *settings = cJSON_AddObjectToObject(root, "calibration");
    cJSON_AddNumberToObject(settings, "revision", copy.revision);
    number(settings, "sample_kg", copy.sample_kg > 0 ? copy.sample_kg : NAN);
    number(settings, "sample_seconds", copy.sample_seconds > 0 ? copy.sample_seconds : NAN);
    number(settings, "feeder_kg_h", calibration_rate(copy.sample_kg, copy.sample_seconds));
    cJSON_AddStringToObject(settings, "scope", "ESP_ONLY_NO_BOILER_COMMAND");
    return root;
}
bool configure(const cJSON *payload, std::string &error) {
    auto *revision = cJSON_GetObjectItemCaseSensitive(payload, "revision");
    auto *kg = cJSON_GetObjectItemCaseSensitive(payload, "sample_kg");
    auto *seconds = cJSON_GetObjectItemCaseSensitive(payload, "sample_seconds");
    if (!cJSON_IsNumber(revision) || !std::isfinite(revision->valuedouble) || revision->valuedouble < 0 ||
        revision->valuedouble >= UINT32_MAX || std::floor(revision->valuedouble) != revision->valuedouble ||
        !cJSON_IsNumber(kg) || !cJSON_IsNumber(seconds) ||
        !((kg->valuedouble == 0 && seconds->valuedouble == 0) || std::isfinite(calibration_rate(kg->valuedouble, seconds->valuedouble)))) {
        error = "Nieprawidlowy pomiar: masa 0.01-25 kg, czas 10-3600 s, wydajnosc 0.01-100 kg/h"; return false;
    }
    if (cJSON_GetArraySize(payload) != 3) { error = "Nieznane lub powtorzone pola kalibracji"; return false; }
    Lock storage(storage_mutex);
    Store candidate;
    {
        Lock lock(mutex);
        if (revision->valuedouble != config.revision) { error = "Kalibracja zmienila sie. Odswiez jej odczyt"; return false; }
        if (kg->valuedouble == config.sample_kg && seconds->valuedouble == config.sample_seconds) return true;
        candidate = config; candidate.totals = meter.totals; ++candidate.revision;
        candidate.sample_kg = kg->valuedouble; candidate.sample_seconds = seconds->valuedouble;
    }
    bool ok = save(candidate);
    {
        Lock lock(mutex); storage_ok = ok;
        if (ok) { config = candidate; meter.discontinuity(); flow = NAN; source = Source::unavailable; checkpoint_at = now(); }
    }
    if (!ok) { error = "Nie zapisano kalibracji w pamieci ESP"; return false; }
    state::log("Pellet: ESP calculation calibration saved; no boiler command");
    return true;
}
bool self_test() {
    Meter counter;
    if (calibration_rate(1, 360) != 10 || std::isfinite(calibration_rate(1, 0)) ||
        std::isfinite(calibration_rate(NAN, 30)) || std::isfinite(calibration_rate(25, 10)) ||
        valid_rate(-1) || valid_rate(INFINITY)) return false;
    counter.sample(0, 3.6, Source::reported); counter.sample(1000, 3.6, Source::reported);
    if (std::fabs(counter.totals.reported_kg - 0.001) > 1e-12) return false;
    counter.sample(11000, 3.6, Source::reported);
    if (counter.totals.omitted_ms != 10000 || counter.totals.counted_ms != 1000) return false;
    counter.sample(12000, NAN, Source::reported); counter.sample(13000, 36, Source::feeder_estimate);
    if (counter.totals.omitted_ms != 12000) return false;
    counter.sample(14000, 0, Source::feeder_estimate);
    if (std::fabs(counter.totals.estimated_kg - 0.005) > 1e-12) return false;
    counter.discontinuity(); counter.sample(15000, 100, Source::reported);
    if (counter.totals.counted_ms != 2000) return false;
    Store stored; if (!valid_store(stored)) return false;
    stored.sample_kg = 1; if (valid_store(stored)) return false;
    stored.sample_seconds = 360; if (!valid_store(stored)) return false;
    stored.totals.reported_kg = NAN; return !valid_store(stored);
}
}
