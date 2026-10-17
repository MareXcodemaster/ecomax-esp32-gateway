#include "self_test_api.hpp"
#include "esp_http_client.h"
#include "board.hpp"
#include "state.hpp"
#include "mbedtls/base64.h"
#include <string>
#include <cstring>
#include <cstdio>

namespace {
struct Response { std::string body, cookie; int status = 0; bool complete = false; };
esp_err_t collect(esp_http_client_event_t *event) {
    auto &response = *static_cast<Response *>(event->user_data);
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
        if (response.body.size() + event->data_len > 512*1024) return ESP_FAIL;
        response.body.append(static_cast<const char *>(event->data), event->data_len);
    } else if (event->event_id == HTTP_EVENT_ON_HEADER && !strcasecmp(event->header_key, "Set-Cookie")) {
        response.cookie = event->header_value;
        response.cookie = response.cookie.substr(0, response.cookie.find(';'));
    }
    return ESP_OK;
}
Response request(const char *path, const char *authorization, const char *cookie = nullptr,
                 const char *body = nullptr, bool guard = false, const char *origin = nullptr) {
    Response response;
    char url[96]; snprintf(url, sizeof(url), "http://127.0.0.1%s", path);
    esp_http_client_config_t config{}; config.url = url; config.timeout_ms = 3000;
    config.event_handler = collect; config.user_data = &response; config.disable_auto_redirect = true;
    auto client = esp_http_client_init(&config);
    if (!client) return response;
    if (authorization) esp_http_client_set_header(client, "Authorization", authorization);
    if (cookie) esp_http_client_set_header(client, "Cookie", cookie);
    if (guard) esp_http_client_set_header(client, "X-Lazar-Request", "1");
    if (origin) esp_http_client_set_header(client, "Origin", origin);
    if (body) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body, strlen(body));
    }
    auto result = esp_http_client_perform(client);
    response.status = esp_http_client_get_status_code(client);
    // This JSON/session API intentionally has no browser Basic-auth challenge.
    // IDF reports a received 401 without WWW-Authenticate as NOT_SUPPORTED.
    response.complete = result == ESP_OK || (response.status == 401 && result == ESP_ERR_NOT_SUPPORTED);
    esp_http_client_cleanup(client);
    return response;
}
}
bool self_test_api() {
    char plain[64], auth[96] = "Basic "; size_t size;
    snprintf(plain, sizeof(plain), "%s:%s", board::user, board::password());
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char *>(auth + 6), sizeof(auth) - 6, &size,
                             reinterpret_cast<unsigned char *>(plain), strlen(plain))) return false;
    auth[6 + size] = 0;
    unsigned passed = 0, total = 0;
    auto check = [&](const char *name, Response result, int expected) {
        ++total;
        bool ok = result.complete && result.status == expected;
        if (ok) ++passed;
        state::log("HTTP self-test %s: %s (status %d expected %d)", name, ok ? "PASS" : "FAIL", result.status, expected);
        return ok;
    };
    check("no-auth", request("/api/status", nullptr), 401);
    check("wrong-auth", request("/api/status", "Basic d3Jvbmc6d3Jvbmc="), 401);
    auto status = request("/api/status", auth);
    check("status", status, 200);
    auto *json = cJSON_Parse(status.body.c_str());
    auto *tx = cJSON_GetObjectItemCaseSensitive(json, "tx_bytes");
    auto *de = cJSON_GetObjectItemCaseSensitive(json, "de_level");
    auto *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    bool passive = cJSON_IsNumber(tx) && tx->valuedouble == 0 && cJSON_IsNumber(de) && de->valueint == 0 &&
                   cJSON_IsString(mode) && !strcmp(mode->valuestring, "PASSIVE_RX_ONLY");
    cJSON_Delete(json); ++total; if (passive) ++passed;
    state::log("HTTP self-test TX lock snapshot: %s", passive ? "PASS" : "FAIL");
    check("capture", request("/api/capture?after=0&limit=2", auth), 200);
    check("logs", request("/api/logs", auth), 200);
    check("wifi-missing-guard", request("/api/wifi", auth, nullptr, "{}"), 403);
    check("foreign-origin", request("/api/wifi", auth, nullptr, "{}", true, "http://other.invalid"), 403);
    check("invalid-wifi", request("/api/wifi", auth, nullptr, "{}", true), 400);
    check("hostname-no-auth", request("/api/hostname", nullptr, nullptr, "{}", true), 401);
    check("hostname-missing-guard", request("/api/hostname", auth, nullptr, "{}"), 403);
    check("hostname-foreign-origin", request("/api/hostname", auth, nullptr, "{}", true, "http://other.invalid"), 403);
    check("hostname-invalid", request("/api/hostname", auth, nullptr, "{\"hostname\":\"bad name\"}", true), 400);
    check("updates-no-auth",request("/api/updates",nullptr),401);
    check("access-no-auth",request("/api/access",nullptr,nullptr,"{}",true),401);
    check("access-missing-guard",request("/api/access",auth,nullptr,"{}"),403);
    check("access-missing-token",request("/api/access",auth,nullptr,"{}",true),403);
    check("access-foreign-origin",request("/api/access",auth,nullptr,"{}",true,"http://other.invalid"),403);
    check("updates-snapshot",request("/api/updates",auth),200);
    check("updates-check-guard",request("/api/updates/check",auth,nullptr,"{}"),403);
    check("updates-install-confirm",request("/api/updates/install",auth,nullptr,"{}",true),400);
    check("invalid-baud", request("/api/sniffer", auth, nullptr, "{\"baud\":123}", true), 400);
    check("boiler-readonly", request("/api/ecomax", auth), 200);
    check("boiler-no-auth", request("/api/ecomax", nullptr), 401);
    check("mixers", request("/api/mixers", auth), 200);
    check("thermostats", request("/api/thermostats", auth), 200);
    check("alarms", request("/api/alarms", auth), 200);
    check("frame-stats", request("/api/frames", auth), 200);
    check("latest-frames", request("/api/latest_frames?source=69&type=177&limit=1", auth), 200);
    check("decoded-latest-frames", request("/api/latest_frames?source=69&type=8&limit=1&decode=1", auth), 200);
    check("parameters-unavailable", request("/api/parameters", auth), 501);
    check("schedules-unavailable", request("/api/schedules", auth), 501);
    check("small-ota-rejected", request("/api/ota", auth, nullptr, "{}", true), 400);
    check("unknown-api", request("/api/not-present", auth), 404);
    check("panel-no-auth",request("/api/state",nullptr),401);
    check("panel-session",request("/api/session",auth),200);
    check("panel-state",request("/api/state",auth),200);
    check("pellet-no-auth", request("/api/pellet", nullptr), 401);
    check("pellet-readonly", request("/api/pellet", auth), 200);
    check("pellet-config-no-auth", request("/api/pellet/calibration", nullptr, nullptr, "{}", true), 401);
    check("pellet-config-missing-guard", request("/api/pellet/calibration", auth, nullptr, "{}"), 403);
    check("pellet-config-missing-token", request("/api/pellet/calibration", auth, nullptr, "{}", true), 403);
    check("pellet-config-foreign-origin", request("/api/pellet/calibration", auth, nullptr, "{}", true, "http://other.invalid"), 403);
    check("job-no-auth",request("/api/job",nullptr),401);
    check("job-invalid-id",request("/api/job",auth),400);
    check("job-unknown-id",request("/api/job?id=00000000000000000000000000000000",auth),409);
    check("panel-write-missing-guard",request("/api/write/execute",auth,nullptr,"{}"),403);
    check("panel-write-missing-token",request("/api/write/execute",auth,nullptr,"{}",true),403);
    check("panel-write-foreign-origin",request("/api/write/execute",auth,nullptr,"{}",true,"http://other.invalid"),403);
    auto login = request("/api/login", auth, nullptr, "", true);
    check("login", login, 200);
    check("session-cookie", request("/api/status", nullptr, login.cookie.c_str()), 200);
    check("logout", request("/api/logout", nullptr, login.cookie.c_str(), "", true), 200);
    check("expired-cookie", request("/api/status", nullptr, login.cookie.c_str()), 401);
    state::log("HTTP LOOPBACK SELF-TEST: %u/%u %s; no boiler TX, no WiFi changes", passed, total, passed == total ? "PASS" : "FAIL");
    return passed == total;
}

