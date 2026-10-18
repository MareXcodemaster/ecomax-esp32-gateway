#include "api.hpp"
#include "board.hpp"
#include "state.hpp"
#include "wifi.hpp"
#include "transport.hpp"
#include "boiler.hpp"
#include "panel.hpp"
#include "panel_history.hpp"
#include "pellet.hpp"
#include "updates.hpp"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "web_asset.hpp"
#include "panel_assets.hpp"
#include <string>
#include <sys/time.h>

namespace api {
static httpd_handle_t server;
static char expected_auth[96];
static char panel_csrf[33];
static std::atomic<bool> push_pending{false};
static esp_timer_handle_t reboot_timer;
constexpr unsigned remembered_slots=8, temporary_slots=4;
struct Session { char digest[65]{}; uint64_t expires=0; bool remembered=false; };
static Session sessions[remembered_slots+temporary_slots];
static unsigned remembered_next=0, temporary_next=0;
struct RememberedStore { uint32_t magic=0x4c475331, next=0; char password_digest[65]{}; char digests[remembered_slots][65]{}; };
static bool equal(const char *left, const char *right) {
    size_t n = strlen(left); if (n != strlen(right)) return false;
    unsigned difference = 0; for (size_t i = 0; i < n; ++i) difference |= left[i] ^ right[i];
    return difference == 0;
}
static bool hex_string(const char *value,size_t length) {
    if(strlen(value)!=length) return false;
    for(size_t i=0;i<length;++i) if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f'))) return false;
    return true;
}
static bool digest(const char *value,char out[65]) {
    uint8_t bytes[32];
    if(mbedtls_sha256(reinterpret_cast<const unsigned char *>(value),strlen(value),bytes,0)!=0) return false;
    for(unsigned i=0;i<sizeof(bytes);++i) snprintf(out+i*2,3,"%02x",bytes[i]);
    return true;
}
static bool save_remembered() {
    RememberedStore store; store.next=remembered_next;
    if(!digest(expected_auth,store.password_digest)) return false;
    for(unsigned i=0;i<remembered_slots;++i) if(sessions[i].remembered) memcpy(store.digests[i],sessions[i].digest,sizeof(store.digests[i]));
    nvs_handle_t handle;
    if(nvs_open("web_auth",NVS_READWRITE,&handle)!=ESP_OK) return false;
    auto result=nvs_set_blob(handle,"remembered",&store,sizeof(store));
    if(result==ESP_OK) result=nvs_commit(handle);
    nvs_close(handle); return result==ESP_OK;
}
static void load_remembered() {
    nvs_handle_t handle; if(nvs_open("web_auth",NVS_READONLY,&handle)!=ESP_OK) return;
    RememberedStore store; size_t size=sizeof(store);
    auto result=nvs_get_blob(handle,"remembered",&store,&size); nvs_close(handle);
    char password_digest[65]{}; store.password_digest[64]=0;
    if(result!=ESP_OK||size!=sizeof(store)||store.magic!=0x4c475331||store.next>=remembered_slots||
       !digest(expected_auth,password_digest)||!equal(store.password_digest,password_digest)) return;
    remembered_next=store.next;
    for(unsigned i=0;i<remembered_slots;++i) {
        store.digests[i][64]=0;
        if(hex_string(store.digests[i],64)) { memcpy(sessions[i].digest,store.digests[i],65); sessions[i].remembered=true; }
    }
}
static bool basic(httpd_req_t *req) {
    char value[96];
    return httpd_req_get_hdr_value_str(req, "Authorization", value, sizeof(value)) == ESP_OK && equal(value, expected_auth);
}
static int cookie_session(httpd_req_t *req) {
    char cookie[512];
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) return -1;
    char *cursor = cookie;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == ';') ++cursor;
        char *end = strchr(cursor, ';'); if (end) *end = 0;
        if (!strncmp(cursor, "lazar_session=", 14)) {
            const char *token = cursor + 14;
            auto now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
            char hash[65]{};
            if(hex_string(token,32)&&digest(token,hash)) for(unsigned i=0;i<remembered_slots+temporary_slots;++i)
                if(*sessions[i].digest&&(sessions[i].remembered||now<sessions[i].expires)&&equal(hash,sessions[i].digest)) return i;
        }
        if (!end) break;
        cursor = end + 1;
    }
    return -1;
}
static esp_err_t message(httpd_req_t *req, const char *status, const char *text) {
    httpd_resp_set_status(req, status); httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    auto *root = cJSON_CreateObject(); cJSON_AddStringToObject(root, "message", text);
    char *body = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    if (!body) return ESP_FAIL;
    auto result = httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN); cJSON_free(body); return result;
}
static bool authorize(httpd_req_t *req, bool mutate = false) {
    if (!basic(req) && cookie_session(req) < 0) {
        message(req, "401 Unauthorized", "Zaloguj sie jako admin"); return false;
    }
    if (mutate) {
        char flag[8];
        if (httpd_req_get_hdr_value_str(req, "X-Lazar-Request", flag, sizeof(flag)) != ESP_OK || strcmp(flag, "1")) {
            message(req, "403 Forbidden", "Missing request guard"); return false;
        }
        char origin[128], host[96], allowed[128];
        if (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) == ESP_OK) {
            if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) return false;
            snprintf(allowed, sizeof(allowed), "http://%s", host);
            if (strcmp(origin, allowed)) { message(req, "403 Forbidden", "Cross-origin request rejected"); return false; }
        }
    }
    return true;
}
static esp_err_t json(httpd_req_t *req, cJSON *value) {
    if (!value) return message(req, "503 Service Unavailable", "Memory unavailable");
    char *body = cJSON_PrintUnformatted(value); cJSON_Delete(value);
    if (!body) return ESP_FAIL;
    httpd_resp_set_type(req, "application/json"); httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    auto result = httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN); cJSON_free(body); return result;
}
static uint64_t query_number(httpd_req_t *req, const char *name, uint64_t fallback) {
    char query[128], value[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, name, value, sizeof(value)) != ESP_OK) return fallback;
    char *end; auto n = strtoull(value, &end, 10);
    return *value && !*end && value[0] != '-' && n < UINT64_MAX - 1 ? n : fallback;
}
static cJSON *request_json(httpd_req_t *req) {
    if (!req->content_len || req->content_len > 1024) { message(req, "400 Bad Request", "JSON limit: 1024 bytes"); return nullptr; }
    char body[1025]; size_t offset = 0; unsigned timeouts = 0;
    while (offset < req->content_len) {
        int n = httpd_req_recv(req, body + offset, req->content_len - offset);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= 2) continue;
        if (n <= 0) { message(req, "408 Request Timeout", "Incomplete request"); return nullptr; }
        offset += n;
    }
    body[offset] = 0;
    const char *end = nullptr;
    auto *value = cJSON_ParseWithLengthOpts(body, offset + 1, &end, true);
    if (!cJSON_IsObject(value)) { cJSON_Delete(value); message(req, "400 Bad Request", "Expected JSON object"); return nullptr; }
    return value;
}
static esp_err_t root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self'; frame-ancestors 'none'; base-uri 'none'");
    httpd_resp_set_hdr(req, "Vary", "Accept-Encoding");
    char encoding[128];
    if (httpd_req_get_hdr_value_str(req, "Accept-Encoding", encoding, sizeof(encoding)) == ESP_OK && strstr(encoding, "gzip")) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        return httpd_resp_send(req, reinterpret_cast<const char *>(web_gzip), sizeof(web_gzip));
    }
    return httpd_resp_send(req, web_page, sizeof(web_page) - 1);
}
static esp_err_t panel_asset(httpd_req_t *req) {
    std::string path=req->uri; path=path.substr(0,path.find('?'));
    for(auto &asset:panel_assets) if(path==asset.path) {
        httpd_resp_set_type(req,asset.mime); httpd_resp_set_hdr(req,"Cache-Control","no-cache");
        httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
        httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self'; frame-ancestors 'none'; base-uri 'none'");
        if(asset.gzip) httpd_resp_set_hdr(req,"Content-Encoding","gzip");
        return httpd_resp_send(req,reinterpret_cast<const char *>(asset.bytes),asset.length);
    }
    return message(req,"404 Not Found","Unknown resource");
}
static esp_err_t panel_state(httpd_req_t *req) {
    if(!authorize(req)) return ESP_OK;
    auto *value=panel::snapshot();
    // A remembered cookie survives restart, but mutation guards must adopt the new boot nonce.
    if(value&&strcmp(req->uri,"/api/export")) cJSON_AddStringToObject(value,"csrf",panel_csrf);
    return json(req,value);
}
static esp_err_t history_state(httpd_req_t *req) {
    if(!authorize(req))return ESP_OK;
    auto from=query_number(req,"from",0),to=query_number(req,"to",0);
    if((from||to)&&(!from||!to||from<1577836800||to>4102444800ULL||to<from||to-from>3*24*3600))return message(req,"400 Bad Request","Zakres historii: maksymalnie 3 dni");
    return json(req,panel_history::snapshot(from,to));
}
static esp_err_t pellet_state(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    return json(req, pellet::snapshot());
}
static esp_err_t pellet_configure(httpd_req_t *req) {
    if (!authorize(req, true)) return ESP_OK;
    char csrf[64];
    if (httpd_req_get_hdr_value_str(req, "X-Tester-Token", csrf, sizeof(csrf)) != ESP_OK || !equal(csrf, panel_csrf))
        return message(req, "403 Forbidden", "Missing panel token");
    auto *payload = request_json(req); if (!payload) return ESP_OK;
    std::string error; bool ok = pellet::configure(payload, error); cJSON_Delete(payload);
    return ok ? json(req, pellet::snapshot()) : message(req, "409 Conflict", error.c_str());
}
static esp_err_t panel_session(httpd_req_t *req) {
    if(!authorize(req)) return ESP_OK;
    auto *value=cJSON_CreateObject(); cJSON_AddStringToObject(value,"csrf",panel_csrf);
    int slot=cookie_session(req); cJSON_AddBoolToObject(value,"remembered",slot>=0&&sessions[slot].remembered);
    return json(req,value);
}
static esp_err_t panel_job(httpd_req_t *req) {
    if(!authorize(req))return ESP_OK;
    char query[96],id[33];
    if(httpd_req_get_url_query_str(req,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"id",id,sizeof(id))!=ESP_OK||!hex_string(id,32))return message(req,"400 Bad Request","Invalid job ID");
    auto *value=panel::job_snapshot(id);
    if(!value)return message(req,"409 Conflict","Operacja zmienila sie. Sprawdz historie i odczyt; nie ponawiaj polecenia");
    cJSON_AddStringToObject(value,"csrf",panel_csrf); return json(req,value);
}
static esp_err_t panel_command(httpd_req_t *req) {
    if(!authorize(req,true)) return ESP_OK;
    if(updates::installing())return message(req,"409 Conflict","Trwa aktualizacja ESP. Zaczekaj na zakonczenie.");
    char csrf[64];
    if(httpd_req_get_hdr_value_str(req,"X-Tester-Token",csrf,sizeof(csrf))!=ESP_OK || !equal(csrf,panel_csrf)) return message(req,"403 Forbidden","Missing panel token");
    auto *payload=request_json(req); if(!payload) return ESP_OK;
    if(!strcmp(req->uri,"/api/panel/clock")) {
        auto *time=cJSON_GetObjectItemCaseSensitive(payload,"epoch"); bool ok=cJSON_IsNumber(time)&&time->valuedouble>=1577836800&&time->valuedouble<=4102444800;
        if(ok) panel::set_clock(time->valuedouble);
        cJSON_Delete(payload); return message(req,ok?"200 OK":"400 Bad Request",ok?"OK":"Invalid clock");
    }
    const char *kind=!strcmp(req->uri,"/api/write/save")?"save":!strcmp(req->uri,"/api/rs485/probe")?"rs_probe":!strcmp(req->uri,"/api/transport")?"transport":!strcmp(req->uri,"/api/connect")?"connect":!strcmp(req->uri,"/api/scan")?"scan":!strcmp(req->uri,"/api/refresh")?"refresh":!strcmp(req->uri,"/api/control/prepare")?"control_prepare":!strcmp(req->uri,"/api/control/execute")?"control_execute":!strcmp(req->uri,"/api/write/prepare")?"prepare":"execute";
    int slot=cookie_session(req); std::string error;
    auto *response=panel::submit(kind,payload,slot>=0?sessions[slot].digest:"basic",error); cJSON_Delete(payload);
    if(!response&&error=="Trwa operacja. Poczekaj na zakonczenie") {
        auto *reply=cJSON_CreateObject();cJSON_AddStringToObject(reply,"error",error.c_str());cJSON_AddBoolToObject(reply,"busy",true);
        httpd_resp_set_status(req,"409 Conflict");return json(req,reply);
    }
    return response?json(req,response):message(req,"409 Conflict",error.c_str());
}
static esp_err_t login(httpd_req_t *req) {
    if (!basic(req)) return message(req, "401 Unauthorized", "Nieprawidlowe haslo");
    if (!authorize(req, true)) return ESP_OK;
    char preference[4]{}; bool remembered=false;
    if(httpd_req_get_hdr_value_len(req,"X-Lazar-Remember")) {
        if(httpd_req_get_hdr_value_str(req,"X-Lazar-Remember",preference,sizeof(preference))!=ESP_OK||
           (strcmp(preference,"0")&&strcmp(preference,"1"))) return message(req,"400 Bad Request","Invalid remember preference");
        remembered=!strcmp(preference,"1");
    }
    int old_slot=cookie_session(req); const auto old_session=old_slot>=0?sessions[old_slot]:Session{};
    unsigned previous_next=remembered_next,previous_temporary=temporary_next,slot;
    if(remembered) {
        slot=remembered_next;
        if(old_slot>=0&&static_cast<unsigned>(old_slot)<remembered_slots) slot=old_slot;
        else for(unsigned i=0;i<remembered_slots;++i) if(!*sessions[i].digest) { slot=i; break; }
        remembered_next=(slot+1)%remembered_slots;
    } else slot=remembered_slots+temporary_next++%temporary_slots;
    auto &session=sessions[slot]; const auto previous=session;
    uint8_t random[16]; esp_fill_random(random, sizeof(random)); char token[33]{};
    const char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < sizeof(random); ++i) { token[i * 2] = hex[random[i] >> 4]; token[i * 2 + 1] = hex[random[i] & 15]; }
    session={};
    if(!digest(token,session.digest)) { session=previous; remembered_next=previous_next; temporary_next=previous_temporary; return message(req,"503 Service Unavailable","Session hash unavailable"); }
    session.remembered=remembered; session.expires=esp_timer_get_time()/1000+24ULL*60*60*1000;
    if(old_slot>=0&&static_cast<unsigned>(old_slot)!=slot) sessions[old_slot]={};
    if((remembered||old_session.remembered)&&!save_remembered()) {
        session=previous; if(old_slot>=0) sessions[old_slot]=old_session;
        remembered_next=previous_next; temporary_next=previous_temporary;
        return message(req,"503 Service Unavailable","Nie zapisano zapamietanego logowania");
    }
    char cookie[160]; snprintf(cookie,sizeof(cookie),"lazar_session=%s; Path=/; HttpOnly; SameSite=Strict%s",token,remembered?"; Max-Age=31536000":"");
    httpd_resp_set_hdr(req, "Set-Cookie", cookie); state::log("WWW login accepted; remembered=%s",remembered?"yes":"no");
    return message(req, "200 OK", "OK");
}
static esp_err_t logout(httpd_req_t *req) {
    if (!authorize(req, true)) return ESP_OK;
    int slot = cookie_session(req);
    if(slot>=0) {
        const auto previous=sessions[slot]; sessions[slot]={};
        if(previous.remembered&&!save_remembered()) { sessions[slot]=previous; return message(req,"503 Service Unavailable","Nie uniewazniono zapamietanego logowania"); }
    }
    httpd_resp_set_hdr(req, "Set-Cookie", "lazar_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
    return message(req, "200 OK", "OK");
}
static esp_err_t status(httpd_req_t *req) { if (!authorize(req)) return ESP_OK; return json(req, state::status()); }
static esp_err_t ping(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    auto *root = cJSON_CreateObject(); cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);
    return json(req, root);
}
static esp_err_t capture(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    unsigned limit = static_cast<unsigned>(std::min(query_number(req, "limit", 16), 32ULL));
    return json(req, state::capture(query_number(req, "after", 0), limit));
}
static esp_err_t logs(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    return json(req, state::logs(query_number(req, "after", 0)));
}
static esp_err_t configure_wifi(httpd_req_t *req) {
    if (!authorize(req, true)) return ESP_OK;
    if (updates::installing() || panel::is_busy()) return message(req, "409 Conflict", "Zaczekaj na zakonczenie operacji panelu lub aktualizacji ESP.");
    auto *body = request_json(req); if (!body) return ESP_OK;
    auto *ssid = cJSON_GetObjectItemCaseSensitive(body, "ssid");
    auto *password = cJSON_GetObjectItemCaseSensitive(body, "password");
    esp_err_t result = ESP_ERR_INVALID_ARG;
    if (cJSON_IsString(ssid) && cJSON_IsString(password)) result = network::configure(ssid->valuestring, password->valuestring);
    cJSON_Delete(body);
    return message(req, result == ESP_OK ? "202 Accepted" : "400 Bad Request", result == ESP_OK ? "Zapisano WiFi. AP pozostaje wlaczony." : esp_err_to_name(result));
}
static esp_err_t wifi_scan(httpd_req_t *req) {
    if(!authorize(req, req->method == HTTP_POST))return ESP_OK;
    if(req->method == HTTP_POST) {
        if(updates::installing())return message(req,"409 Conflict","Trwa aktualizacja ESP.");
        if(panel::is_busy())return message(req,"409 Conflict","Zaczekaj na zakonczenie operacji panelu.");
        network::request_scan();
        return message(req,"202 Accepted","Wyszukiwanie sieci Wi-Fi 2,4 GHz.");
    }
    return json(req,network::scan_results());
}
static esp_err_t configure_hostname(httpd_req_t *req) {
    if (!authorize(req, true)) return ESP_OK;
    auto *body = request_json(req); if (!body) return ESP_OK;
    auto *name = cJSON_GetObjectItemCaseSensitive(body, "hostname");
    auto result = cJSON_IsString(name) ? network::configure_hostname(name->valuestring) : ESP_ERR_INVALID_ARG;
    auto *reply = cJSON_CreateObject();
    cJSON_AddStringToObject(reply, "message", result == ESP_OK ? "Zapisano nazwe. Otworz nowy adres .local; IP pozostaje bez zmian." : esp_err_to_name(result));
    if (result == ESP_OK) {
        char url[80]; snprintf(url, sizeof(url), "http://%s.local/", name->valuestring);
        cJSON_AddStringToObject(reply, "local_url", url);
    }
    cJSON_Delete(body);
    httpd_resp_set_status(req, result == ESP_OK ? "202 Accepted" : "400 Bad Request");
    return json(req, reply);
}
static esp_err_t configure_sniffer(httpd_req_t *req) {
    if (!authorize(req, true)) return ESP_OK;
    auto *body = request_json(req); if (!body) return ESP_OK;
    auto *baud = cJSON_GetObjectItemCaseSensitive(body, "baud");
    bool ok = cJSON_IsNumber(baud) && baud->valuedouble == baud->valueint && transport::set_baud(baud->valueint);
    cJSON_Delete(body); return message(req, ok ? "202 Accepted" : "400 Bad Request", ok ? "RX baud updated; TX remains disabled" : "Unsupported baud");
}
static esp_err_t ecomax(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    return json(req, boiler::snapshot());
}
static esp_err_t frame_stats(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    return json(req, state::frame_stats());
}
static esp_err_t latest_frames(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    auto source = query_number(req, "source", 256), type = query_number(req, "type", 256);
    auto offset = std::min(query_number(req, "offset", 0), 128ULL);
    return json(req, state::latest_frames(source < 256 ? static_cast<int>(source) : -1, type < 256 ? static_cast<int>(type) : -1,
        offset, std::min(query_number(req, "limit", 8), 16ULL), query_number(req, "decode", 0) == 1));
}
static esp_err_t subset(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    auto *data = boiler::snapshot();
    if (!data) return message(req, "503 Service Unavailable", "Memory unavailable");
    const char *key = !strcmp(req->uri, "/api/mixers") ? "mixers" : "thermostats";
    auto *items = cJSON_DetachItemFromObjectCaseSensitive(data, key);
    cJSON_DeleteItemFromObjectCaseSensitive(data, "sensors");
    cJSON_DeleteItemFromObjectCaseSensitive(data, "outputs");
    cJSON_DeleteItemFromObjectCaseSensitive(data, "modules");
    cJSON_DeleteItemFromObjectCaseSensitive(data, "frame_versions");
    cJSON_DeleteItemFromObjectCaseSensitive(data, "mixers"); cJSON_DeleteItemFromObjectCaseSensitive(data, "thermostats");
    cJSON_AddItemToObject(data, "items", items ? items : cJSON_CreateArray());
    return json(req, data);
}
static esp_err_t alarms(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    auto *data = boiler::snapshot(); auto *result = cJSON_CreateObject();
    for (auto *key : {"available", "fresh", "age_ms"}) {
        auto *value = cJSON_GetObjectItemCaseSensitive(data, key);
        if (value) cJSON_AddItemToObject(result, key, cJSON_Duplicate(value, true));
    }
    auto *sensors = cJSON_GetObjectItemCaseSensitive(data, "sensors");
    auto *count = cJSON_GetObjectItemCaseSensitive(sensors, "pending_alerts");
    cJSON_AddItemToObject(result, "pending_count", count ? cJSON_Duplicate(count, true) : cJSON_CreateNull());
    cJSON_AddBoolToObject(result, "history_available", false); cJSON_Delete(data);
    return json(req, result);
}
static esp_err_t unavailable(httpd_req_t *req) {
    if (!authorize(req)) return ESP_OK;
    return message(req, "501 Not Implemented", "Pelny katalog parametrow/harmonogramow nie jest zaimplementowany. Nadawanie do kotla jest wylaczone.");
}
static esp_err_t configure_access(httpd_req_t *req){
    if(!authorize(req,true))return ESP_OK;
    char csrf[64];
    if(httpd_req_get_hdr_value_str(req,"X-Tester-Token",csrf,sizeof(csrf))!=ESP_OK||!equal(csrf,panel_csrf))return message(req,"403 Forbidden","Missing panel token");
    if(panel::is_busy())return message(req,"409 Conflict","Zaczekaj na zakonczenie operacji panelu");
    auto *body=request_json(req);if(!body)return ESP_OK;
    auto *current=cJSON_GetObjectItemCaseSensitive(body,"current_password"),*web=cJSON_GetObjectItemCaseSensitive(body,"password"),*ap=cJSON_GetObjectItemCaseSensitive(body,"ap_password"),*confirm=cJSON_GetObjectItemCaseSensitive(body,"confirm");
    updates::FlashGuard flash_guard;
    auto result=ESP_ERR_INVALID_STATE;
    if(flash_guard.acquired&&cJSON_IsString(current)&&cJSON_IsString(web)&&(cJSON_IsNull(ap)||cJSON_IsString(ap))&&cJSON_IsTrue(confirm))
        result=device_access::configure(current->valuestring,web->valuestring,cJSON_IsString(ap)?ap->valuestring:nullptr);
    for(auto *item:{current,web,ap})if(cJSON_IsString(item))mbedtls_platform_zeroize(item->valuestring,strlen(item->valuestring));
    cJSON_Delete(body);
    if(result!=ESP_OK)return message(req,result==ESP_ERR_INVALID_ARG?"400 Bad Request":"409 Conflict","Nie zapisano hasel. Sprawdz obecne haslo, zakres i stan aktualizacji.");
    pellet::checkpoint(true);flash_guard.keep=true;
    message(req,"200 OK","Hasla zapisane. Restart ESP za 2 sekundy; zaloguj sie nowym haslem.");
    ESP_ERROR_CHECK(esp_timer_start_once(reboot_timer,2000000));return ESP_OK;
}
static esp_err_t update_status(httpd_req_t *req) {
    if(!authorize(req))return ESP_OK;
    return json(req,updates::snapshot());
}
static esp_err_t update_command(httpd_req_t *req) {
    if(!authorize(req,true))return ESP_OK;
    if(!strcmp(req->uri,"/api/updates/check")){
        auto result=updates::check();return message(req,result==ESP_OK?"202 Accepted":"409 Conflict",result==ESP_OK?"Sprawdzanie wydan GitHub.":"Trwa inna operacja aktualizatora.");
    }
    auto *body=request_json(req);if(!body)return ESP_OK;
    esp_err_t result=ESP_ERR_INVALID_ARG;
    if(!strcmp(req->uri,"/api/updates/config")){
        auto *automatic=cJSON_GetObjectItemCaseSensitive(body,"automatic"),*beta=cJSON_GetObjectItemCaseSensitive(body,"include_beta");
        if(cJSON_IsBool(automatic)&&cJSON_IsBool(beta))result=updates::configure(cJSON_IsTrue(automatic),cJSON_IsTrue(beta));
    }else{
        auto *version=cJSON_GetObjectItemCaseSensitive(body,"version"),*confirm=cJSON_GetObjectItemCaseSensitive(body,"confirm");
        if(cJSON_IsString(version)&&cJSON_IsTrue(confirm))result=updates::install(version->valuestring);
    }
    cJSON_Delete(body);return message(req,result==ESP_OK?"202 Accepted":result==ESP_ERR_INVALID_ARG?"400 Bad Request":"409 Conflict",
        result==ESP_OK?"Przyjeto. Nie ponawiaj polecenia automatycznie.":result==ESP_ERR_INVALID_ARG?"Nieprawidlowe dane lub brak potwierdzenia.":"Aktualizacja niedostepna: sprawdz wersje, zgodnosc instalacji i stan operacji.");
}
static esp_err_t ota(httpd_req_t *req) {
    if (!authorize(req, true)) return ESP_OK;
    if(panel::is_busy()) return message(req,"409 Conflict","Wait for the panel operation before OTA");
    const auto *partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || req->content_len < 1024 || req->content_len > partition->size)
        return message(req, "400 Bad Request", "Use application OTA .bin, not merged flash image");
    updates::FlashGuard flash_guard;
    if(!flash_guard.acquired)return message(req,"409 Conflict","Another firmware update is running");
    esp_ota_handle_t handle;
    auto result = esp_ota_begin(partition, req->content_len, &handle);
    if (result != ESP_OK) return message(req, "409 Conflict", esp_err_to_name(result));
    uint8_t buffer[2048]; size_t received = 0; unsigned timeouts = 0;
    state::log("OTA start, size=%u; passive RX task remains running", (unsigned)req->content_len);
    while (received < req->content_len) {
        int n = httpd_req_recv(req, reinterpret_cast<char *>(buffer), std::min(sizeof(buffer), req->content_len - received));
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= 3) continue;
        if (n <= 0) { esp_ota_abort(handle); state::log("OTA aborted: incomplete upload"); return message(req, "408 Request Timeout", "OTA upload incomplete"); }
        result = esp_ota_write(handle, buffer, n);
        if (result != ESP_OK) { esp_ota_abort(handle); return message(req, "400 Bad Request", esp_err_to_name(result)); }
        received += n;
    }
    result = esp_ota_end(handle);
    if (result != ESP_OK) { state::log("OTA validation failed: %s", esp_err_to_name(result)); return message(req, "400 Bad Request", esp_err_to_name(result)); }
    esp_app_desc_t description{};
    result = esp_ota_get_partition_description(partition, &description);
    if (result != ESP_OK || strcmp(description.project_name, "lazar_stage0"))
        return message(req, "400 Bad Request", "Firmware belongs to a different project");
    result = esp_ota_set_boot_partition(partition);
    if (result != ESP_OK) return message(req, "500 Internal Server Error", esp_err_to_name(result));
    pellet::checkpoint(true);
    state::log("OTA image validated; restarting in 2 seconds");
    flash_guard.keep=true;
    message(req, "200 OK", "Obraz sprawdzony. Restart za 2 sekundy.");
    ESP_ERROR_CHECK(esp_timer_start_once(reboot_timer, 2000000)); return ESP_OK;
}
static esp_err_t websocket(httpd_req_t *req) {
    if (req->method == HTTP_GET) { if (!authorize(req)) return ESP_FAIL; return ESP_OK; }
    httpd_ws_frame_t frame{};
    auto result = httpd_ws_recv_frame(req, &frame, 0);
    if (result != ESP_OK || frame.len > 128) return ESP_FAIL;
    uint8_t bytes[129]; frame.payload = bytes;
    return frame.len ? httpd_ws_recv_frame(req, &frame, frame.len) : ESP_OK;
}
static void push(void *) {
    size_t count = 8; int clients[8];
    auto *value = state::status(); char *body = cJSON_PrintUnformatted(value); cJSON_Delete(value);
    if (body && httpd_get_client_list(server, &count, clients) == ESP_OK) {
        httpd_ws_frame_t frame{}; frame.type = HTTPD_WS_TYPE_TEXT;
        frame.payload = reinterpret_cast<uint8_t *>(body); frame.len = strlen(body);
        for (size_t i = 0; i < count; ++i) {
            if (httpd_ws_get_fd_info(server, clients[i]) == HTTPD_WS_CLIENT_WEBSOCKET &&
                httpd_ws_send_frame_async(server, clients[i], &frame) != ESP_OK) httpd_sess_trigger_close(server, clients[i]);
        }
    }
    if (body) cJSON_free(body);
    push_pending.store(false);
}
static void push_task(void *) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!push_pending.exchange(true) && httpd_queue_work(server, push, nullptr) != ESP_OK) push_pending.store(false);
    }
}
static esp_err_t missing(httpd_req_t *req, httpd_err_code_t) {
    if (!strncmp(req->uri, "/api/", 5)) return message(req, "404 Not Found", "Unknown API endpoint");
    httpd_resp_set_status(req, "302 Found"); httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, "", 0);
}
void start() {
    uint8_t nonce[16]; esp_fill_random(nonce,sizeof(nonce)); for(unsigned i=0;i<16;++i) snprintf(panel_csrf+2*i,3,"%02x",nonce[i]);
    char plain[64]; snprintf(plain, sizeof(plain), "%s:%s", board::user, board::password());
    snprintf(expected_auth, sizeof(expected_auth), "Basic "); size_t written;
    configASSERT(mbedtls_base64_encode(reinterpret_cast<unsigned char *>(expected_auth + 6), sizeof(expected_auth) - 6,
                                     &written, reinterpret_cast<unsigned char *>(plain), strlen(plain)) == 0);
    expected_auth[6 + written] = 0;
    char known_hash[65]{};
    configASSERT(digest("abc",known_hash)&&equal(known_hash,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    load_remembered();
    esp_timer_create_args_t timer{}; timer.callback = [](void *) { esp_restart(); }; timer.name = "ota_reboot";
    ESP_ERROR_CHECK(esp_timer_create(&timer, &reboot_timer));
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); config.stack_size = 16384;
    config.max_uri_handlers = 56; config.max_open_sockets = 8; config.lru_purge_enable = true;
    config.uri_match_fn=httpd_uri_match_wildcard;
    config.recv_wait_timeout = 5; config.send_wait_timeout = 12;
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    auto add = [](const char *uri, httpd_method_t method, esp_err_t (*fn)(httpd_req_t *), bool ws = false) {
        httpd_uri_t entry{}; entry.uri = uri; entry.method = method; entry.handler = fn; entry.is_websocket = ws;
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &entry));
    };
    add("/", HTTP_GET, panel_asset); add("/diagnostics", HTTP_GET, root); add("/api/login", HTTP_POST, login); add("/api/logout", HTTP_POST, logout);
    add("/api/status", HTTP_GET, status); add("/api/ping", HTTP_GET, ping);
    add("/api/capture", HTTP_GET, capture); add("/api/logs", HTTP_GET, logs);
    add("/api/wifi", HTTP_POST, configure_wifi); add("/api/hostname", HTTP_POST, configure_hostname); add("/api/sniffer", HTTP_POST, configure_sniffer);
    add("/api/wifi/scan",HTTP_GET,wifi_scan);add("/api/wifi/scan",HTTP_POST,wifi_scan);
    add("/api/ota", HTTP_POST, ota); add("/api/ecomax", HTTP_GET, ecomax);
    add("/api/updates",HTTP_GET,update_status);add("/api/updates/check",HTTP_POST,update_command);
    add("/api/access",HTTP_POST,configure_access);
    add("/api/updates/config",HTTP_POST,update_command);add("/api/updates/install",HTTP_POST,update_command);
    add("/api/mixers", HTTP_GET, subset); add("/api/thermostats", HTTP_GET, subset);
    add("/api/alarms", HTTP_GET, alarms); add("/api/frames", HTTP_GET, frame_stats);
    add("/api/latest_frames", HTTP_GET, latest_frames);
    add("/api/parameters", HTTP_GET, unavailable); add("/api/schedules", HTTP_GET, unavailable);
    add("/ws", HTTP_GET, websocket, true);
    add("/api/state",HTTP_GET,panel_state); add("/api/export",HTTP_GET,panel_state); add("/api/session",HTTP_GET,panel_session);
    add("/api/job",HTTP_GET,panel_job);
    add("/api/history",HTTP_GET,history_state);
    add("/api/pellet", HTTP_GET, pellet_state); add("/api/pellet/calibration", HTTP_POST, pellet_configure);
    for(auto uri:{"/api/connect","/api/scan","/api/refresh","/api/write/prepare","/api/write/execute","/api/write/save","/api/control/prepare","/api/control/execute","/api/panel/clock","/api/transport","/api/rs485/probe"}) add(uri,HTTP_POST,panel_command);
    add("/*",HTTP_GET,panel_asset);
    ESP_ERROR_CHECK(httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, missing));
    configASSERT(xTaskCreate(push_task, "status_push", 3072, nullptr, 3, nullptr) == pdPASS);
    state::log("HTTP+WebSocket ready; local boiler decoder, diagnostics and OTA; all boiler TX disabled");
}
}

