#include "device_access.hpp"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <string>

namespace {
std::string stored, pending;
bool entropy = false, commit_failure = false;
unsigned random_calls = 0;
}
esp_err_t nvs_open(const char* name, int, nvs_handle_t* handle) {
    assert(!std::strcmp(name, "device_access")); *handle = 1; return ESP_OK;
}
esp_err_t nvs_get_str(nvs_handle_t, const char*, char* value, size_t* length) {
    if (stored.empty()) return ESP_ERR_NVS_NOT_FOUND;
    assert(stored.size() + 1 <= *length);
    std::memcpy(value, stored.c_str(), stored.size() + 1);
    *length = stored.size() + 1; return ESP_OK;
}
esp_err_t nvs_set_str(nvs_handle_t, const char*, const char* value) { pending = value; return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t) {
    if (commit_failure) return ESP_FAIL;
    stored = pending; return ESP_OK;
}
void nvs_close(nvs_handle_t) {}
void bootloader_random_enable() { assert(!entropy); entropy = true; }
void bootloader_random_disable() { assert(entropy); entropy = false; }
void esp_fill_random(void* destination, size_t length) {
    assert(entropy && length == 16); ++random_calls;
    auto* bytes = static_cast<unsigned char*>(destination);
    for (size_t i = 0; i < length; ++i) bytes[i] = static_cast<unsigned char>(i + random_calls);
}
void access_tests() {
    assert(device_access::init() == ESP_OK);
    assert(!entropy && random_calls == 1 && stored.size() == 32);
    const std::string first = device_access::password();
    assert(first == stored);
    assert(device_access::init() == ESP_OK && first == device_access::password());
    assert(random_calls == 1);
    stored = "corrupt";
    assert(device_access::init() == ESP_ERR_INVALID_STATE);
    assert(!*device_access::password());
    stored.clear(); commit_failure = true;
    assert(device_access::init() == ESP_FAIL);
    assert(!*device_access::password() && !entropy);
    commit_failure = false;
    assert(device_access::init() == ESP_OK);
    assert(first != device_access::password());
}
