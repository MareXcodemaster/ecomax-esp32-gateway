#include "wifi.hpp"
#include "board.hpp"
#include "state.hpp"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs.h"
#include "mdns.h"
#include "network_defaults.hpp"
#include "network_name.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include <atomic>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace network {
struct Credentials { char ssid[33]{}, password[65]{}; };
static Credentials saved;
static SemaphoreHandle_t config_mutex;
static std::atomic<bool> pending{true}, connected{false}, reconnect{false};
static std::atomic<int> reason{0};
static esp_netif_t *sta;
static bool mdns_ready = false, mdns_started = false;
static char ap_name[33];
static char saved_hostname[64];
static std::atomic<bool> hostname_pending{false};
esp_err_t configure_hostname(const char *hostname) {
    if (!network_name::valid(hostname)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    auto result = nvs_open("lazar_s0", NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    result = nvs_set_str(handle, "hostname", hostname);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    snprintf(saved_hostname, sizeof(saved_hostname), "%s", hostname);
    xSemaphoreGive(config_mutex);
    hostname_pending.store(true); return ESP_OK;
}
static std::atomic<bool> scan_pending{false}, scanning{false};
static wifi_ap_record_t scan_records[16]{};
static uint16_t scan_count = 0, scan_total = 0;
static esp_err_t scan_error = ESP_ERR_INVALID_STATE;
static unsigned scan_sequence = 0;
void request_scan() { scan_pending.store(true); }
cJSON *scan_results() {
    auto *value = cJSON_CreateObject();
    cJSON_AddStringToObject(value, "command", "scan_results");
    cJSON_AddBoolToObject(value, "busy", scan_pending.load() || scanning.load());
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    cJSON_AddNumberToObject(value, "sequence", scan_sequence);
    cJSON_AddStringToObject(value, "result", esp_err_to_name(scan_error));
    cJSON_AddNumberToObject(value, "total", scan_total);
    auto *items = cJSON_AddArrayToObject(value, "networks");
    for (unsigned i = 0; i < scan_count; ++i) {
        const auto &record = scan_records[i]; char ssid[33]{}, bssid[18];
        memcpy(ssid, record.ssid, sizeof(record.ssid));
        snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
            record.bssid[0], record.bssid[1], record.bssid[2], record.bssid[3], record.bssid[4], record.bssid[5]);
        auto *item = cJSON_CreateObject(); cJSON_AddItemToArray(items, item);
        cJSON_AddStringToObject(item, "ssid", ssid); cJSON_AddStringToObject(item, "bssid", bssid);
        cJSON_AddNumberToObject(item, "rssi", record.rssi);
        cJSON_AddNumberToObject(item, "channel", record.primary);
        cJSON_AddNumberToObject(item, "authmode", record.authmode);
    }
    xSemaphoreGive(config_mutex); return value;
}
static void event(void *, esp_event_base_t base, int32_t id, void *data) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        connected.store(true); reconnect.store(false); reason.store(0);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        connected.store(false); reconnect.store(true);
        reason.store(static_cast<wifi_event_sta_disconnected_t *>(data)->reason);
    }
}
esp_err_t configure(const char *ssid, const char *password) {
    if (!ssid || !password || strlen(ssid) > 32 || strlen(password) > 64) return ESP_ERR_INVALID_ARG;
    if (*password && strlen(password) < 8) return ESP_ERR_INVALID_ARG;
    if (provision::fixed && (strcmp(ssid, provision::ssid) || strcmp(password, provision::password)))
        return ESP_ERR_NOT_SUPPORTED;
    Credentials value; snprintf(value.ssid, sizeof(value.ssid), "%s", ssid);
    snprintf(value.password, sizeof(value.password), "%s", password);
    nvs_handle_t handle;
    esp_err_t result = nvs_open("lazar_s0", NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    // Store the pair together so an interrupted commit cannot combine old/new credentials.
    result = nvs_set_blob(handle, "wifi", &value, sizeof(value));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    xSemaphoreTake(config_mutex, portMAX_DELAY); saved = value; xSemaphoreGive(config_mutex);
    pending.store(true); return ESP_OK;
}
static void wifi_task(void *) {
    Credentials current; bool trying = false; unsigned attempts = 0;
    char hostname[64]; snprintf(hostname, sizeof(hostname), "%s", saved_hostname);
    esp_err_t hostname_error = ESP_OK;
    uint64_t next = 0;
    for (;;) {
        auto now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
        if (hostname_pending.exchange(false)) {
            xSemaphoreTake(config_mutex, portMAX_DELAY);
            snprintf(hostname, sizeof(hostname), "%s", saved_hostname);
            xSemaphoreGive(config_mutex);
            hostname_error = esp_netif_set_hostname(sta, hostname);
            if (mdns_started) {
                auto error = mdns_hostname_set(hostname);
                mdns_ready = error == ESP_OK;
                if (error != ESP_OK) hostname_error = error;
            } else if (hostname_error == ESP_OK) hostname_error = ESP_ERR_INVALID_STATE;
            state::log("Local hostname applied: %s.local (%s); WiFi remains connected", hostname, esp_err_to_name(hostname_error));
        }
        if (pending.exchange(false)) {
            xSemaphoreTake(config_mutex, portMAX_DELAY); current = saved; xSemaphoreGive(config_mutex);
            esp_wifi_disconnect(); connected.store(false); reconnect.store(false); trying = false; attempts = 0;
            wifi_config_t config{};
            memcpy(config.sta.ssid, current.ssid, strlen(current.ssid));
            memcpy(config.sta.password, current.password, strlen(current.password));
            config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
            config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
            config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
            auto err = esp_wifi_set_config(WIFI_IF_STA, &config);
            state::log("WiFi configuration applied (%s); AP remains enabled", err == ESP_OK ? "OK" : esp_err_to_name(err));
            next = now + 1000;
        }
        if (scan_pending.exchange(false)) {
            scanning.store(true);
            // Scanning and connecting cannot run together. Keep the setup AP enabled.
            if (!connected.load()) {
                esp_wifi_disconnect(); trying = false;
                vTaskDelay(pdMS_TO_TICKS(500)); reconnect.store(false);
            }
            wifi_scan_config_t scan{};
            scan.scan_time.active.min = 100; scan.scan_time.active.max = 150;
            auto error = esp_wifi_scan_start(&scan, true);
            xSemaphoreTake(config_mutex, portMAX_DELAY);
            scan_count = 0; scan_total = 0;
            if (error == ESP_OK) error = esp_wifi_scan_get_ap_num(&scan_total);
            if (error == ESP_OK) {
                scan_count = 16;
                error = esp_wifi_scan_get_ap_records(&scan_count, scan_records);
            }
            if (error != ESP_OK) { scan_count = 0; esp_wifi_clear_ap_list(); }
            scan_error = error; ++scan_sequence;
            xSemaphoreGive(config_mutex);
            scanning.store(false);
            now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
            next = now + 5000;
            state::log("WiFi diagnostic scan: %s, %u APs", esp_err_to_name(error), scan_total);
        }
        if (reconnect.exchange(false)) {
            trying = false; next = now + std::min(30000U, 2000U << std::min(attempts, 4U));
            state::log("STA disconnected, reason=%d; reconnect in %llu ms", reason.load(), (unsigned long long)(next - now));
        }
        if (*current.ssid && !connected.load() && !trying && now >= next) {
            ++attempts;
            auto err = esp_wifi_connect(); trying = err == ESP_OK;
            next = now + 30000;
            state::log("STA connect attempt %u: %s", attempts, esp_err_to_name(err));
        }
        if (trying && !connected.load() && now >= next) { esp_wifi_disconnect(); trying = false; }
        state::Network value;
        snprintf(value.ap, sizeof(value.ap), "%s", ap_name);
        snprintf(value.ssid, sizeof(value.ssid), "%s", current.ssid);
        snprintf(value.hostname, sizeof(value.hostname), "%s", hostname);
        value.hostname_pending = hostname_pending.load(); value.hostname_error = hostname_error;
        value.connected = connected.load(); value.reason = reason.load(); value.retries = attempts;
        value.fixed = provision::fixed; value.mdns = mdns_ready && value.connected;
        wifi_ap_record_t info;
        if (value.connected && esp_wifi_sta_get_ap_info(&info) == ESP_OK) {
            value.rssi = info.rssi; value.channel = info.primary;
            esp_netif_ip_info_t ip{};
            if (esp_netif_get_ip_info(sta, &ip) == ESP_OK) snprintf(value.ip, sizeof(value.ip), IPSTR, IP2STR(&ip.ip));
        } else {
            uint8_t primary; wifi_second_chan_t secondary;
            if (esp_wifi_get_channel(&primary, &secondary) == ESP_OK) value.channel = primary;
        }
        auto previous = state::network(); state::network(value);
        if (value.connected && !previous.connected)
            state::log("STA online: http://%s / http://%s.local/ RSSI=%d dBm", value.ip, value.hostname, value.rssi);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
static void dns_task(void *) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(53);
    if (sock < 0 || bind(sock, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        if (sock >= 0) close(sock);
        state::log("DNS unavailable; use http://192.168.4.1"); vTaskDelete(nullptr); return;
    }
    uint8_t buffer[512];
    for (;;) {
        sockaddr_in peer{}; socklen_t size = sizeof(peer);
        int n = recvfrom(sock, buffer, sizeof(buffer) - 16, 0, reinterpret_cast<sockaddr *>(&peer), &size);
        // Reply only to setup-network A/IN questions; do not act as a DNS server on the home LAN.
        if (n < 17 || (ntohl(peer.sin_addr.s_addr) & 0xffffff00U) != 0xc0a80400U ||
            (buffer[2] & 0x80) || buffer[4] || buffer[5] != 1) continue;
        size_t end = 12;
        while (end < static_cast<size_t>(n) && buffer[end]) {
            unsigned length = buffer[end];
            if (length > 63 || end + 1 + length >= static_cast<size_t>(n)) { end = n; break; }
            end += length + 1;
        }
        if (end + 5 != static_cast<size_t>(n)) continue;
        if (buffer[end + 1] || buffer[end + 2] != 1 || buffer[end + 3] || buffer[end + 4] != 1) continue;
        buffer[2] = 0x81; buffer[3] = 0x80; buffer[6] = 0; buffer[7] = 1;
        buffer[8] = buffer[9] = buffer[10] = buffer[11] = 0;
        const uint8_t answer[] = {0xc0,0x0c,0,1,0,1,0,0,0,30,0,4,192,168,4,1};
        memcpy(buffer + n, answer, sizeof(answer));
        sendto(sock, buffer, n + sizeof(answer), 0, reinterpret_cast<sockaddr *>(&peer), size);
    }
}
void start() {
    config_mutex = xSemaphoreCreateMutex(); configASSERT(config_mutex);
    snprintf(saved_hostname, sizeof(saved_hostname), "%s", board::hostname);
    nvs_handle_t handle;
    if (nvs_open("lazar_s0", NVS_READONLY, &handle) == ESP_OK) {
        size_t size = sizeof(saved);
        if (nvs_get_blob(handle, "wifi", &saved, &size) != ESP_OK || size != sizeof(saved)) saved = {};
        saved.ssid[32] = saved.password[64] = 0;
        char hostname[64]{}; size_t hostname_size = sizeof(hostname);
        if (nvs_get_str(handle, "hostname", hostname, &hostname_size) == ESP_OK && network_name::valid(hostname))
            snprintf(saved_hostname, sizeof(saved_hostname), "%s", hostname);
        nvs_close(handle);
    }
    if (provision::fixed) {
        snprintf(saved.ssid, sizeof(saved.ssid), "%s", provision::ssid);
        snprintf(saved.password, sizeof(saved.password), "%s", provision::password);
        state::log("Fixed WiFi preset: %s; automatic connection on every boot", saved.ssid);
    }
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    auto *ap_netif = esp_netif_create_default_wifi_ap(); sta = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(sta, saved_hostname));
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, nullptr));
    uint8_t mac[6]; ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    snprintf(ap_name, sizeof(ap_name), "Piec-Setup-%02X%02X", mac[4], mac[5]);
    wifi_config_t ap{};
    memcpy(ap.ap.ssid, ap_name, strlen(ap_name));
    ap.ap.ssid_len = strlen(ap_name); ap.ap.channel = 1; ap.ap.max_connection = 4;
    snprintf(reinterpret_cast<char *>(ap.ap.password), sizeof(ap.ap.password), "%s", board::setup_password());
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK; ap.ap.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start()); ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    auto mdns_error = mdns_init();
    if (mdns_error == ESP_OK) {
        // Advertise only on the home LAN, not the fallback setup interface.
        mdns_error = mdns_netif_action(ap_netif,
            static_cast<mdns_event_actions_t>(MDNS_EVENT_DISABLE_IP4 | MDNS_EVENT_DISABLE_IP6));
        if (mdns_error == ESP_OK) mdns_error = mdns_hostname_set(saved_hostname);
        if (mdns_error == ESP_OK) mdns_error = mdns_instance_name_set("ecoMAX Wi-Fi");
        if (mdns_error == ESP_OK) mdns_error = mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
        mdns_ready = mdns_started = mdns_error == ESP_OK;
        if (!mdns_ready) mdns_free();
    }
    state::log("mDNS %s.local on STA: %s; address requires router connection", saved_hostname, esp_err_to_name(mdns_error));
    state::log("Setup AP=%s; http://192.168.4.1; WWW user=%s; passwords omitted", ap_name, board::user);
    configASSERT(xTaskCreate(wifi_task, "wifi_manager", 4096, nullptr, 4, nullptr) == pdPASS);
    configASSERT(xTaskCreate(dns_task, "setup_dns", 3072, nullptr, 3, nullptr) == pdPASS);
}
}

