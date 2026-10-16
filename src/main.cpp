#include "board.hpp"
#include "state.hpp"
#include "transport.hpp"
#include "wifi.hpp"
#include "api.hpp"
#include "self_test_api.hpp"
#include "usb_console.hpp"
#include "boiler.hpp"
#include "protocol.hpp"
#include "panel.hpp"
#include "pellet.hpp"
#include "updates.hpp"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main() {
    transport::lock_tx();
    // Catalogue nodes must not compete with Wi-Fi/tasks for internal RAM.
    cJSON_Hooks json_hooks{};
    json_hooks.malloc_fn = [](size_t bytes) -> void * {
        void *value=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        return value?value:heap_caps_malloc(bytes,MALLOC_CAP_8BIT);
    };
    json_hooks.free_fn=heap_caps_free;
    cJSON_InitHooks(&json_hooks);
    state::init(); boiler::init(); protocol::init(); state::log("LazarGateway %s; reset_reason=%d", board::version, esp_reset_reason());
    bool passed = transport::self_test();
    state::log("Parser self-test (fragmentation, bad XOR, length, timeout, resync): %s", passed ? "PASS" : "FAIL");
    configASSERT(passed);
    passed = boiler::self_test();
    state::log("Sensor decoder self-test (synthetic frames + every truncation): %s", passed ? "PASS" : "FAIL");
    configASSERT(passed);
    passed = protocol::self_test();
    state::log("Protocol self-test (schema, regdata, parameters, bounds, version mismatch): %s", passed ? "PASS" : "FAIL");
    configASSERT(passed);
    auto result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(device_access::init());
    configASSERT(pellet::self_test()); pellet::init();
    state::log("Pellet meter self-test: PASS; ESP calculations only, no boiler calibration command");
    transport::start(); network::start(); panel::start(); updates::init(); api::start();
    configASSERT(self_test_api());
    state::log("BOOT REQUIRED SELF-TESTS: PASS; parser/sensors/protocol/pellet/panel guard/HTTP assertions completed");
    usb_console::start();
    const auto *partition = esp_ota_get_running_partition(); esp_ota_img_states_t image_state;
    if (esp_ota_get_state_partition(partition, &image_state) == ESP_OK && image_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback()); state::log("OTA boot self-test passed; image confirmed");
    }
    updates::start();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        pellet::checkpoint();
        auto *status = state::status(); auto *text = cJSON_PrintUnformatted(status); cJSON_Delete(status);
        if (text) { printf("[STATUS] %s\n", text); cJSON_free(text); }
    }
}

