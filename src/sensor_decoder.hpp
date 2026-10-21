#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>

// Wire layout follows PyPlumIO structures/sensor_data.py and frame_versions.py (MIT).
namespace plum {
constexpr const char *temperatures[] = {
    "heating_temp", "feeder_temp", "water_heater_temp", "outside_temp", "return_temp",
    "exhaust_temp", "optical_temp", "upper_buffer_temp", "lower_buffer_temp",
    "upper_solar_temp", "lower_solar_temp", "fireplace_temp", "total_gain",
    "hydraulic_coupler_temp", "exchanger_temp", "air_in_temp", "air_out_temp"
};
constexpr const char *outputs[] = {
    "fan", "feeder", "heating_pump", "water_heater_pump", "circulation_pump",
    "lighter", "alarm", "outer_boiler", "fan2_exhaust", "feeder2", "outer_feeder",
    "solar_pump", "fireplace_pump", "gcz_contact", "blow_fan1", "blow_fan2"
};
constexpr const char *modules[] = {"module_a", "module_b", "module_c", "ecolambda", "ecoster", "panel"};
struct Thermostat {
    bool available = false, contact = false, schedule = false;
    uint8_t state = 0;
    float current = NAN, target = NAN;
};
struct Mixer { bool available = false, pump = false; float current = NAN; int target = -1; };
struct FuelLevel { int raw=-1,percent=-1,calibrating=-1; };
inline FuelLevel decode_fuel_level(int raw,bool calibration_encoding) {
    if(raw<0||raw>255)return {};
    if(raw==255)return {raw,-1,-1};
    if(raw<=100)return {raw,raw,0};
    // The captured LZ panel uses values above 100 as CAL, not a percentage offset.
    return calibration_encoding?FuelLevel{raw,-1,1}:FuelLevel{raw,raw-101,-1};
}
struct Sensors {
    uint16_t versions[256]{};
    bool version_present[256]{};
    uint8_t state = 0, transmission = 0, thermostat = 0, pending_alerts = 0;
    uint32_t output_bits = 0, output_flags = 0;
    float temperature[17];
    int heating_target = -1, heating_status = -1, water_heater_target = -1, water_heater_status = -1;
    int fuel_level = -1, boiler_load = -1, lambda_state = -1, lambda_target = -1;
    int fuel_level_raw=-1,fuel_calibrating=-1;
    float fan_power = NAN, boiler_power = NAN, fuel_consumption = NAN, lambda_level = NAN;
    char module_version[6][24]{};
    Thermostat thermostats[16];
    Mixer mixers[16];
    uint8_t thermostat_slots = 0, mixer_slots = 0;
    Sensors() { for (auto &value : temperature) value = NAN; }
};
class Reader {
    const uint8_t *bytes; size_t size;
public:
    size_t pos = 0; bool ok = true;
    Reader(const uint8_t *data, size_t length) : bytes(data), size(length) {}
    uint8_t u8() { if (pos >= size) { ok = false; return 0; } return bytes[pos++]; }
    int optional() { auto value = u8(); return value == 255 ? -1 : value; }
    uint16_t u16() { auto lo = u8(); return lo | (static_cast<uint16_t>(u8()) << 8); }
    uint32_t u32() { auto lo = u16(); return lo | (static_cast<uint32_t>(u16()) << 16); }
    float f32() {
        static_assert(sizeof(float) == 4);
        auto bits = u32(); float value; std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value) ? value : NAN;
    }
    void skip(size_t count) {
        if (count > size - pos) { ok = false; pos = size; } else pos += count;
    }
};
inline bool decode_sensors(const uint8_t *bytes, size_t size, Sensors &result) {
    Reader r(bytes, size); Sensors s;
    auto versions = r.u8();
    for (unsigned i = 0; i < versions && r.ok; ++i) {
        auto type = r.u8(); auto version = r.u16();
        s.version_present[type] = true; s.versions[type] = version;
    }
    s.state = r.u8(); s.output_bits = r.u32(); s.output_flags = r.u32();
    auto count = r.u8();
    for (unsigned i = 0; i < count && r.ok; ++i) {
        auto index = r.u8(); auto value = r.f32();
        if (index < 17) s.temperature[index] = value;
    }
    s.heating_target = r.optional(); s.heating_status = r.optional();
    s.water_heater_target = r.optional(); s.water_heater_status = r.optional();
    s.pending_alerts = r.u8(); r.skip(s.pending_alerts);
    s.fuel_level_raw = r.u8();
    s.transmission = r.u8(); s.fan_power = r.f32(); s.boiler_load = r.optional();
    s.boiler_power = r.f32(); s.fuel_consumption = r.f32(); s.thermostat = r.u8();
    for (unsigned i = 0; i < 6; ++i) {
        auto major = r.u8();
        if (major == 255) continue;
        auto minor = r.u8(); auto patch = r.u8();
        if (i == 0) {
            auto vendor = r.u8(); auto revision = r.u8();
            if (vendor < 32 || vendor > 126) return false;
            std::snprintf(s.module_version[i], sizeof(s.module_version[i]), "%u.%u.%u.%c%u", major, minor, patch, vendor, revision);
        } else std::snprintf(s.module_version[i], sizeof(s.module_version[i]), "%u.%u.%u", major, minor, patch);
    }
    s.lambda_state = r.optional();
    if (s.lambda_state >= 0) { s.lambda_target = r.optional(); s.lambda_level = r.u16() / 10.0f; }
    auto contacts = r.u8();
    if (contacts != 255) {
        s.thermostat_slots = r.u8();
        if (s.thermostat_slots > 16) return false;
        for (unsigned i = 0; i < s.thermostat_slots; ++i) {
            auto &t = s.thermostats[i]; t.state = r.u8(); t.current = r.f32(); t.target = r.f32();
            t.available = std::isfinite(t.current) && std::isfinite(t.target) && t.target > 0;
            t.contact = i < 8 && (contacts & (1U << i));
            t.schedule = i < 5 && (contacts & (1U << (i + 3)));
        }
    }
    s.mixer_slots = r.u8();
    if (s.mixer_slots > 16) return false;
    for (unsigned i = 0; i < s.mixer_slots; ++i) {
        auto &m = s.mixers[i]; m.current = r.f32(); m.target = r.optional(); r.skip(1);
        m.pump = r.u8() & 1; r.skip(1); m.available = std::isfinite(m.current);
    }
    if (!r.ok || r.pos != size) return false;
    auto fuel=decode_fuel_level(s.fuel_level_raw,!std::strcmp(s.module_version[0],"8.41.38.L0")&&!std::strcmp(s.module_version[5],"8.30.28"));
    s.fuel_level=fuel.percent;s.fuel_calibrating=fuel.calibrating;
    result = s; return true;
}
}
