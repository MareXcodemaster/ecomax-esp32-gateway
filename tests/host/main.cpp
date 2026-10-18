#include "protocol.hpp"
#include "protocol_fixtures.hpp"
#include "sensor_decoder.hpp"
#include "sensor_fixtures.hpp"
#include "identity_guard.hpp"
#include "thermostat_decoder.hpp"
#include "network_name.hpp"
#include "wifi_retry.hpp"
#include "update_manifest.hpp"
#include <string>
#include <cassert>
#include <cstdio>
void access_tests();
void diagnostics_tests();

int main() {
    assert(wifi_retry::delay(0)==1000&&wifi_retry::delay(1)==1000);
    assert(wifi_retry::delay(2)==2000&&wifi_retry::delay(3)==4000);
    assert(wifi_retry::delay(4)==8000&&wifi_retry::delay(5)==15000);
    assert(wifi_retry::delay(1000)==15000);
    assert(update_manifest::newer("0.7.0","0.7.0-beta.3"));
    assert(update_manifest::newer("0.10.0","0.9.9"));
    assert(update_manifest::newer("0.7.0-beta.4","0.7.0-beta.3"));
    assert(update_manifest::newer("0.7.0-beta.10","0.7.0-beta.9"));
    assert(!update_manifest::newer("0.7.0-beta.9","0.7.0-beta.10"));
    assert(!update_manifest::newer("0.7.0-beta.4","0.7.0"));
    assert(!update_manifest::newer("0.7.0-beta.4","0.7.0-beta.4"));
    assert(!update_manifest::newer("0.7.0-beta.8","0.7.0-beta.8"));
    assert(!update_manifest::newer("0.7.0-beta.7","0.7.0-beta.8"));
    assert(update_manifest::newer("0.7.0-beta.9","0.7.0-beta.8"));
    assert(!update_manifest::newer("0.7.0","0.7.0"));
    assert(!update_manifest::newer("0.6.99","0.7.0"));
    update_manifest::Version version;
    for(const auto *bad: {"", "0", "0.7", "0.7.0.1", "01.7.0", "65536.0.0", "0.7.0-beta", "0.7.0+test"})
        assert(!update_manifest::version(bad,version,true));
    assert(update_manifest::download_url("https://release-assets.githubusercontent.com/example"));
    for(const auto *bad: {"http://github.com/file", "https://github.com.evil.invalid/file", "https://release-assets.githubusercontent.com.evil.invalid/file", "file:///tmp/firmware", "https://github.com/other/repo/releases/download/v1/file"})
        assert(!update_manifest::download_url(bad));
    auto *release=cJSON_Parse(R"JSON({"prerelease":false,"draft":false,"tag_name":"v0.7.0","assets":[{"name":"firmware-ota.bin","size":200000,"digest":"sha256:0000000000000000000000000000000000000000000000000000000000000000","browser_download_url":"https://github.com/MareXcodemaster/ecomax-esp32-gateway/releases/download/v0.7.0/firmware-ota.bin"}]})JSON");
    update_manifest::Candidate candidate;
    assert(update_manifest::parse(release,candidate)&&candidate.size==200000&&!strcmp(candidate.version,"0.7.0"));
    cJSON_ReplaceItemInObjectCaseSensitive(release,"prerelease",cJSON_CreateTrue());
    assert(!update_manifest::parse(release,candidate));
    cJSON_ReplaceItemInObjectCaseSensitive(release,"prerelease",cJSON_CreateFalse());
    auto *assets=cJSON_GetObjectItemCaseSensitive(release,"assets"),*asset=cJSON_GetArrayItem(assets,0);
    cJSON_ReplaceItemInObjectCaseSensitive(asset,"size",cJSON_CreateNumber(200000.5));
    assert(!update_manifest::parse(release,candidate));
    cJSON_ReplaceItemInObjectCaseSensitive(asset,"size",cJSON_CreateNumber(0x300000));
    assert(!update_manifest::parse(release,candidate));
    cJSON_ReplaceItemInObjectCaseSensitive(asset,"size",cJSON_CreateNumber(200000));
    cJSON_ReplaceItemInObjectCaseSensitive(asset,"digest",cJSON_CreateString("sha256:bad"));
    assert(!update_manifest::parse(release,candidate));
    cJSON_ReplaceItemInObjectCaseSensitive(asset,"digest",cJSON_CreateString("sha256:0000000000000000000000000000000000000000000000000000000000000000"));
    cJSON_AddItemToArray(assets,cJSON_Duplicate(asset,true));
    assert(!update_manifest::parse(release,candidate));
    cJSON_Delete(release);
    assert(!update_manifest::parse(nullptr,candidate));
    auto *betas=cJSON_Parse(R"JSON([{"prerelease":true,"draft":false,"tag_name":"v0.7.0-beta.10","assets":[{"name":"firmware-ota.bin","size":200000,"digest":"sha256:0000000000000000000000000000000000000000000000000000000000000000","browser_download_url":"https://github.com/MareXcodemaster/ecomax-esp32-gateway/releases/download/v0.7.0-beta.10/firmware-ota.bin"}]}])JSON");
    assert(update_manifest::select(betas,candidate,true)&&candidate.beta&&!strcmp(candidate.version,"0.7.0-beta.10"));
    assert(!update_manifest::select(betas,candidate,false));
    update_manifest::ReleasePages pages;
    auto *older=cJSON_Duplicate(betas,true);
    auto *older_release=cJSON_GetArrayItem(older,0);
    cJSON_ReplaceItemInObjectCaseSensitive(older_release,"tag_name",cJSON_CreateString("v0.7.0-beta.9"));
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(older_release,"assets"),0),"browser_download_url",cJSON_CreateString("https://github.com/MareXcodemaster/ecomax-esp32-gateway/releases/download/v0.7.0-beta.9/firmware-ota.bin"));
    for(unsigned i=1;i<pages.page_size;++i)cJSON_AddItemToArray(older,cJSON_Duplicate(older_release,true));
    assert(pages.append(older)&&pages.found&&!pages.complete&&!strcmp(pages.best.version,"0.7.0-beta.9"));
    assert(pages.append(betas)&&pages.complete&&!strcmp(pages.best.version,"0.7.0-beta.10"));
    assert(!pages.can_request()&&!pages.append(betas));
    update_manifest::ReleasePages limited;
    for(unsigned i=0;i<limited.max_pages;++i)assert(limited.append(older));
    assert(!limited.complete&&!limited.can_request()&&!limited.append(betas));
    update_manifest::ReleasePages empty;
    auto *empty_page=cJSON_CreateArray();assert(empty.append(empty_page)&&empty.complete&&!empty.found);cJSON_Delete(empty_page);
    update_manifest::ReleasePages malformed;assert(!malformed.append(nullptr)&&malformed.page==1);
    auto *bad_page=cJSON_CreateArray();cJSON_AddItemToArray(bad_page,cJSON_CreateObject());
    assert(malformed.append(bad_page)&&malformed.complete&&!malformed.found);cJSON_Delete(bad_page);
    cJSON_Delete(older);
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(betas,0),"draft",cJSON_CreateTrue());
    assert(!update_manifest::select(betas,candidate,true));cJSON_Delete(betas);
    assert(network_name::valid("piec") && network_name::valid("kotlownia-2"));
    assert(network_name::valid(std::string(63, 'a').c_str()));
    for (const auto *bad : {"", "-piec", "piec-", "dwa piece", "PIEC", "piec.local", "localhost", "x/y"})
        assert(!network_name::valid(bad));
    assert(!network_name::valid(nullptr) && !network_name::valid(std::string(64, 'a').c_str()));
    access_tests();
    using identity_guard::Action;
    assert(identity_guard::decide("", "EXAMPLE123", true) == Action::enroll);
    assert(identity_guard::decide("EXAMPLE123", "EXAMPLE123", true) == Action::match);
    assert(identity_guard::decide("EXAMPLE123", "OTHER123", true) == Action::reject);
    assert(identity_guard::decide("", "EXAMPLE123", false) == Action::reject);
    assert(identity_guard::decide("", "bad input", true) == Action::reject);
    assert(identity_guard::decide("", "", true) == Action::reject);
    protocol::init();
    diagnostics_tests();
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
