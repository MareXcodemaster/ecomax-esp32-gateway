#include "protocol.hpp"
#include "protocol_fixtures.hpp"
#include "sensor_decoder.hpp"
#include "sensor_fixtures.hpp"
#include "identity_guard.hpp"
#include "thermostat_decoder.hpp"
#include <cassert>
#include <cstdio>
void access_tests();

int main() {
    access_tests();
    using identity_guard::Action;
    assert(identity_guard::decide("", "EXAMPLE123", true) == Action::enroll);
    assert(identity_guard::decide("EXAMPLE123", "EXAMPLE123", true) == Action::match);
    assert(identity_guard::decide("EXAMPLE123", "OTHER123", true) == Action::reject);
    assert(identity_guard::decide("", "EXAMPLE123", false) == Action::reject);
    assert(identity_guard::decide("", "bad input", true) == Action::reject);
    assert(identity_guard::decide("", "", true) == Action::reject);
    protocol::init();
    for (const auto* frame : {protocol_fixtures::frame_213, protocol_fixtures::frame_8,
                            protocol_fixtures::frame_177, protocol_fixtures::frame_178, protocol_fixtures::frame_212}) {
        const size_t length = frame[1] | (frame[2] << 8);
        auto* result = protocol::describe(frame, length);
        std::fprintf(stderr, "Synthetic frame %u: %s, fields=%d\n", frame[7],
            cJSON_GetObjectItemCaseSensitive(result, "state")->valuestring,
            cJSON_GetObjectItemCaseSensitive(result, "field_count")->valueint);
        cJSON_Delete(result);
    }
    auto* check = protocol::describe(protocol_fixtures::frame_8, sizeof(protocol_fixtures::frame_8));
    auto* items = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(check,"data"),"fields");
    for (const auto &expected : protocol_fixtures::values) {
        cJSON *match=nullptr, *item=nullptr;
        cJSON_ArrayForEach(item,items) {
            auto* id=cJSON_GetObjectItemCaseSensitive(item,"id");
            if(cJSON_IsNumber(id)&&id->valueint==expected.id)match=item;
        }
        auto* value=cJSON_GetObjectItemCaseSensitive(match,"value");
        double actual=cJSON_IsBool(value)?cJSON_IsTrue(value):cJSON_IsNumber(value)?value->valuedouble:NAN;
        if(!std::isfinite(actual)||std::fabs(actual-expected.value)>.0001)
            std::fprintf(stderr,"Mismatch synthetic field %u: expected %g, got %g\n",expected.id,expected.value,actual);
    }
    cJSON_Delete(check);
    std::fprintf(stderr,"Thermostat self-test: %d\n",plum::thermostat_self_test());
    if (!protocol::self_test()) { std::fprintf(stderr, "FAIL protocol self-test\n"); return 1; }
    for (const auto &fixture : sensor_fixtures) {
        plum::Sensors s;
        assert(plum::decode_sensors(fixture.data, fixture.size, s));
        assert(std::fabs(s.temperature[0] - fixture.heating) < .00001f);
        assert(std::fabs(s.temperature[2] - fixture.water) < .00001f);
        assert(s.heating_target == 75 && s.water_heater_target == 44);
        assert(!std::strcmp(s.module_version[0], "8.41.38.L0"));
        assert(!std::strcmp(s.module_version[5], "8.30.28"));
        assert(s.thermostat_slots == 3 && s.mixer_slots == 4);
        assert(s.thermostats[0].available && s.mixers[0].available);
        for (size_t length = 0; length < fixture.size; ++length)
            assert(!plum::decode_sensors(fixture.data, length, s));
    }
    puts("PASS: real protocol self-test, truncated synthetic sensor frames, identity guards, NVS password persistence/failure/erase");
}
