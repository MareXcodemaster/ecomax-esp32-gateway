#include "usb_console.hpp"
#include "wifi.hpp"
#include "state.hpp"
#include "boiler.hpp"
#include "board.hpp"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/platform_util.h"
#include <cstdio>
#include <cstring>

namespace usb_console {
static void reply(cJSON *value) {
    char *text = cJSON_PrintUnformatted(value); cJSON_Delete(value);
    if (text) { printf("[USB] %s\n", text); cJSON_free(text); }
}
static void result(const char *command, esp_err_t error) {
    auto *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "command", command);
    cJSON_AddBoolToObject(value, "ok", error == ESP_OK);
    cJSON_AddStringToObject(value, "result", esp_err_to_name(error)); reply(value);
}
static void command(char *line) {
    auto *value = cJSON_Parse(line);
    mbedtls_platform_zeroize(line, 512);
    auto *cmd = cJSON_GetObjectItemCaseSensitive(value, "command");
    if (!cJSON_IsString(cmd)) { cJSON_Delete(value); result("invalid", ESP_ERR_INVALID_ARG); return; }
    if (!strcmp(cmd->valuestring, "wifi")) {
        auto *ssid = cJSON_GetObjectItemCaseSensitive(value, "ssid");
        auto *password = cJSON_GetObjectItemCaseSensitive(value, "password");
        auto error = cJSON_IsString(ssid) && cJSON_IsString(password) ?
            network::configure(ssid->valuestring, password->valuestring) : ESP_ERR_INVALID_ARG;
        // Do not echo the received credentials into USB output or the log ring.
        if (cJSON_IsString(password)) mbedtls_platform_zeroize(password->valuestring, strlen(password->valuestring));
        result("wifi", error);
    } else if (!strcmp(cmd->valuestring, "setup")) {
        auto *setup=cJSON_CreateObject();
        cJSON_AddStringToObject(setup,"command","setup");
        cJSON_AddStringToObject(setup,"username",board::user);
        cJSON_AddStringToObject(setup,"password",board::password());
        cJSON_AddStringToObject(setup,"ap_password",board::setup_password());
        cJSON_AddNumberToObject(setup,"access_schema",1);
        cJSON_AddStringToObject(setup,"setup_url","http://192.168.4.1/diagnostics");
        reply(setup);
    } else if (!strcmp(cmd->valuestring, "status")) reply(state::status());
    else if (!strcmp(cmd->valuestring, "ecomax")) reply(boiler::snapshot());
    else if (!strcmp(cmd->valuestring, "frames")) reply(state::frame_stats());
    else if (!strcmp(cmd->valuestring, "latest")) {
        auto *source = cJSON_GetObjectItemCaseSensitive(value, "source");
        auto *type = cJSON_GetObjectItemCaseSensitive(value, "type");
        auto *offset = cJSON_GetObjectItemCaseSensitive(value, "offset");
        int s = cJSON_IsNumber(source) && source->valuedouble == source->valueint && source->valueint >= 0 && source->valueint <= 255 ? source->valueint : -1;
        int t = cJSON_IsNumber(type) && type->valuedouble == type->valueint && type->valueint >= 0 && type->valueint <= 255 ? type->valueint : -1;
        unsigned o = cJSON_IsNumber(offset) && offset->valuedouble == offset->valueint && offset->valueint >= 0 && offset->valueint <= 128 ? offset->valueint : 0;
        reply(state::latest_frames(s, t, o, 4));
    }
    else if (!strcmp(cmd->valuestring, "logs")) reply(state::logs(0));
    else if (!strcmp(cmd->valuestring, "capture")) reply(state::capture(0, 16));
    else if (!strcmp(cmd->valuestring, "scan")) { network::request_scan(); result("scan", ESP_OK); }
    else if (!strcmp(cmd->valuestring, "scan_results")) reply(network::scan_results());
    else result("unsupported", ESP_ERR_NOT_SUPPORTED);
    cJSON_Delete(value);
}
static void receive(void *) {
    char line[512]{}; size_t used = 0; bool overflow = false;
    uint8_t bytes[128]; TickType_t last = xTaskGetTickCount();
    for (;;) {
        int n = usb_serial_jtag_read_bytes(bytes, sizeof(bytes), pdMS_TO_TICKS(200));
        auto now = xTaskGetTickCount();
        if ((used || overflow) && now - last > pdMS_TO_TICKS(3000)) {
            mbedtls_platform_zeroize(line, sizeof(line)); used = 0; overflow = false;
        }
        if (n <= 0) continue;
        last = now;
        for (int i = 0; i < n; ++i) {
            if (bytes[i] == '\n' || bytes[i] == '\r') {
                if (overflow) result("invalid", ESP_ERR_INVALID_SIZE);
                else if (used) { line[used] = 0; command(line); }
                used = 0; overflow = false;
            } else if (!overflow) {
                if (used < sizeof(line) - 1 && bytes[i] >= 0x20) line[used++] = bytes[i];
                else { overflow = true; mbedtls_platform_zeroize(line, sizeof(line)); }
            }
        }
        mbedtls_platform_zeroize(bytes, sizeof(bytes));
    }
}
void start() {
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config{}; config.rx_buffer_size = 1024; config.tx_buffer_size = 256;
        ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
    }
    // Keep stdout in its default polled mode so a disconnected USB host cannot block logging.
    configASSERT(xTaskCreate(receive, "usb_config", 4096, nullptr, 3, nullptr) == pdPASS);
    state::log("USB local setup ready: WiFi configuration and read-only diagnostics; no RS485 TX commands");
}
}

