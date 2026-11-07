#include "tailnet.hpp"
#include "tailnet_guard.hpp"
#include "network_name.hpp"
#include "wifi.hpp"
#include <ctime>
#include "runtime_stats.hpp"
#include "state.hpp"
#include "updates.hpp"
#include "microlink.h"
extern "C" {
#include "ml_policy.h"
}
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "mbedtls/platform_util.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include <atomic>
#include <cstring>
#include <cstdio>

namespace {
std::atomic<bool> ingress{false};
std::atomic<uint32_t> vpn_address{0};
std::atomic<uint32_t> requested_generation{1},active_generation{0};
std::atomic<uint32_t> accepted_packets{0},rejected_packets{0};
}
// MicroLink calls this only after authenticated/decrypted Tailnet policy checks.
extern "C" err_t ecomax_tailnet_input(pbuf *p,netif *n){
    uint8_t header[80]{};const auto bytes=pbuf_copy_partial(p,header,sizeof(header),0);
    if(!tailnet_guard::packet(header,bytes,p->tot_len,vpn_address.load(),ingress.load()&&requested_generation.load()==active_generation.load())){rejected_packets.fetch_add(1);pbuf_free(p);return ERR_OK;}
    pbuf_realloc(p,tailnet_guard::u16(header+2));
    const auto result=tcpip_input(p,n);
    if(result==ERR_OK)accepted_packets.fetch_add(1);else rejected_packets.fetch_add(1);
    return result;
}

namespace tailnet {
namespace {
constexpr uint32_t magic=0x544e0001;
constexpr uint32_t stack_bytes=8192;
struct Config{uint32_t schema=magic;uint8_t enabled=0;char name[64]="piec";char key[193]{};};
struct Status{bool running=false,connected=false,authority=false;int peers=0;uint32_t ip=0,minimum_internal=UINT32_MAX;char phase[32]="disabled",stage[32]{},last_error[32]{};int error_code=0,error_detail=0;};
Config saved;
Status status;
SemaphoreHandle_t mutex=nullptr;
TaskHandle_t worker=nullptr;
portMUX_TYPE worker_mux=portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> ready{false},dirty{false},latched{false};
struct Lock{Lock(){xSemaphoreTake(mutex,portMAX_DELAY);}~Lock(){xSemaphoreGive(mutex);}};
struct Wipe{Config &c;~Wipe(){mbedtls_platform_zeroize(c.key,sizeof(c.key));}};
void phase(const char *s,bool running=false){Lock l;snprintf(status.phase,sizeof(status.phase),"%s",s);status.running=running;status.connected=false;status.authority=false;status.ip=0;status.peers=0;}
void coord(const char *stage,int code,int detail){
    if(!mutex)return;
    Lock l;
    snprintf(status.stage,sizeof(status.stage),"%s",stage);
    if(code){snprintf(status.last_error,sizeof(status.last_error),"%s",stage);status.error_code=code;status.error_detail=detail;}
}
void close_gate(){ingress.store(false);vpn_address.store(0);}
void pause(const char *reason){
    Lock l;
    // A new, committed configuration must not be swallowed by an older failure.
    if(requested_generation.load()!=active_generation.load())return;
    latched.store(true);status.running=status.connected=status.authority=false;status.ip=0;status.peers=0;snprintf(status.phase,sizeof(status.phase),"%s",reason);
}
void task(void *){
    microlink_t *client=nullptr;Config active;Wipe wipe{active};bool stalled=false;
    uint64_t started=0;unsigned low=0;
    for(;;){
        runtime_stats::sample(runtime_stats::Task::tailnet,stack_bytes);
        if(dirty.exchange(false)){
            close_gate();phase("stopping",client!=nullptr);
            if(client){
                // The library never frees a context still used by a worker.
                if(microlink_stop(client)!=ESP_OK){stalled=true;latched.store(true);phase("restart_required",true);continue;}
                else {microlink_destroy(client);client=nullptr;ml_policy_reset();stalled=false;}
            }
            {Lock l;active=saved;active_generation.store(requested_generation.load());}
            low=0;
        }
        if(stalled){vTaskDelay(pdMS_TO_TICKS(1000));continue;}
        if(!active.enabled){phase("disabled");break;}
        if(latched.load())break;
        const bool online=state::network().connected;
        if(!client){
            if(!online){phase("waiting_wifi");vTaskDelay(pdMS_TO_TICKS(1000));continue;}
            if(updates::worker_busy()){phase("waiting_update");vTaskDelay(pdMS_TO_TICKS(1000));continue;}
            if(time(nullptr)<1704067200){network::request_time();phase("waiting_time");vTaskDelay(pdMS_TO_TICKS(1000));continue;}
            const auto free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            const auto block=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            const auto psram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(!tailnet_guard::can_start(free,block,psram)){pause("insufficient_memory");break;}
            phase("starting",true);
            microlink_config_t config{};config.auth_key=active.key;config.device_name=active.name;
            config.enable_derp=config.enable_stun=config.enable_disco=true;config.max_peers=CONFIG_ML_MAX_PEERS;
            config.advertise_routes="";config.ctrl_host="https://controlplane.tailscale.com";config.wifi_tx_power_dbm=0;
            client=microlink_init(&config);
            if(!client){pause("start_failed");break;}
            if(microlink_start(client)!=ESP_OK){
                close_gate();
                if(microlink_stop(client)!=ESP_OK){stalled=true;latched.store(true);phase("restart_required",true);continue;}
                microlink_destroy(client);client=nullptr;ml_policy_reset();pause("start_failed");break;
            }
            started=esp_timer_get_time()/1000;
        }
        ml_policy_status_t policy{};ml_policy_get_status(&policy);
        const bool connected=online&&microlink_is_connected(client);
        const auto ip=connected?microlink_get_vpn_ip(client):0;
        const bool accepted=connected&&policy.authority_ready&&tailnet_guard::vpn_ip(ip);
        vpn_address.store(ip);ingress.store(accepted);
        const auto state=microlink_get_state(client);
        {Lock l;status.running=true;status.connected=connected;status.authority=accepted;status.ip=ip;status.peers=microlink_get_peer_count(client);
         snprintf(status.phase,sizeof(status.phase),"%s",!online?"waiting_wifi":accepted?"connected":connected?"waiting_policy":state==ML_STATE_REGISTERING?"registering":state==ML_STATE_ERROR?"connection_error":"connecting");}
        const auto free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        const auto psram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        {Lock l;if(free<status.minimum_internal)status.minimum_internal=free;}
        low=tailnet_guard::memory_low(free,psram)?low+1:0;
        const bool timeout=!connected&&static_cast<uint64_t>(esp_timer_get_time()/1000)-started>180000;
        if(low>=3||timeout){
            close_gate();phase("stopping",true);
            if(microlink_stop(client)!=ESP_OK){stalled=true;latched.store(true);phase("restart_required",true);continue;}
            microlink_destroy(client);client=nullptr;ml_policy_reset();
            pause(low>=3?"memory_paused":"connect_timeout");
            state::log("Tailnet paused: %s; local boiler panel remains active",low>=3?"memory reserve":"connection timeout");break;
        }
        if(connected)started=esp_timer_get_time()/1000;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    mbedtls_platform_zeroize(active.key,sizeof(active.key));
    runtime_stats::stopped(runtime_stats::Task::tailnet);
    portENTER_CRITICAL(&worker_mux);worker=nullptr;portEXIT_CRITICAL(&worker_mux);
    vTaskDelete(nullptr);
}
}
void init(){
    mutex=xSemaphoreCreateMutex();if(!mutex)return;
    nvs_handle_t h;Config value;Wipe wipe{value};size_t size=sizeof(value);
    if(nvs_open("tailnet",NVS_READONLY,&h)==ESP_OK){auto result=nvs_get_blob(h,"config",&value,&size);nvs_close(h);
        if(result==ESP_OK&&size==sizeof(value)&&value.schema==magic&&value.enabled<=1&&
           memchr(value.name,0,sizeof(value.name))&&memchr(value.key,0,sizeof(value.key))&&
           network_name::valid(value.name)&&(!value.key[0]||tailnet_guard::auth_key(value.key)))saved=value;}
    const auto cause=esp_reset_reason();
    if(saved.enabled&&(cause==ESP_RST_PANIC||cause==ESP_RST_INT_WDT||cause==ESP_RST_TASK_WDT||cause==ESP_RST_WDT)){
        latched.store(true);phase("paused_after_fault");
    }else{dirty.store(saved.enabled);phase(saved.enabled?"waiting_wifi":"disabled");}
}
void activate(){ready.store(true);}
void tick(){
    if(!mutex||!ready.load()||latched.load()||!dirty.load())return;
    portENTER_CRITICAL(&worker_mux);bool start=worker==nullptr;if(start)worker=reinterpret_cast<TaskHandle_t>(1);portEXIT_CRITICAL(&worker_mux);
    if(!start)return;
    TaskHandle_t created=nullptr;
    if(xTaskCreate(task,"tailnet",stack_bytes,nullptr,1,&created)!=pdPASS){phase("start_failed");latched.store(true);portENTER_CRITICAL(&worker_mux);worker=nullptr;portEXIT_CRITICAL(&worker_mux);}
    // The sentinel is sufficient: only the worker clears its ownership.
}
bool configure(const cJSON *body,std::string &error){
    if(!mutex){error="Brak pamieci dla konfiguracji Tailnet";return false;}
    if(updates::installing()){error="Trwa aktualizacja ESP";return false;}
    if(!cJSON_IsObject(body)){error="Nieprawidlowa konfiguracja";return false;}
    Config next;Wipe wipe{next};{Lock l;next=saved;}
    const cJSON *p=nullptr;cJSON_ArrayForEach(p,body){if(strcmp(p->string,"enabled")&&strcmp(p->string,"name")&&strcmp(p->string,"auth_key")){error="Nieznane pole konfiguracji";return false;}}
    auto *enabled=cJSON_GetObjectItemCaseSensitive(body,"enabled"),*name=cJSON_GetObjectItemCaseSensitive(body,"name"),*key=cJSON_GetObjectItemCaseSensitive(body,"auth_key");
    if(!cJSON_IsBool(enabled)||!cJSON_IsString(name)||!network_name::valid(name->valuestring)){error="Podaj nazwe: male litery, cyfry i myslnik (1-63 znaki)";return false;}
    if(key&&(!cJSON_IsString(key)||(key->valuestring[0]&&!tailnet_guard::auth_key(key->valuestring)))){error="Nieprawidlowy klucz autoryzacyjny Tailscale";return false;}
    next.enabled=cJSON_IsTrue(enabled);snprintf(next.name,sizeof(next.name),"%s",name->valuestring);
    if(key&&key->valuestring[0])snprintf(next.key,sizeof(next.key),"%s",key->valuestring);
    if(next.enabled&&!next.key[0]){error="Wpisz klucz autoryzacyjny z konta Tailscale";return false;}
    nvs_handle_t h;auto result=nvs_open("tailnet",NVS_READWRITE,&h);
    if(result==ESP_OK){result=nvs_set_blob(h,"config",&next,sizeof(next));if(result==ESP_OK)result=nvs_commit(h);nvs_close(h);}
    if(result!=ESP_OK){error="Nie zapisano konfiguracji Tailnet w pamieci ESP";return false;}
    close_gate();{Lock l;saved=next;requested_generation.fetch_add(1);status={};snprintf(status.phase,sizeof(status.phase),"%s",next.enabled?"starting":"stopping");latched.store(false);dirty.store(true);}
    return true;
}
cJSON *snapshot(){
    auto *out=cJSON_CreateObject();if(!out)return nullptr;
    cJSON_AddBoolToObject(out,"experimental",true);
    if(!mutex){cJSON_AddStringToObject(out,"phase","unavailable");return out;}
    Status s;bool enabled,key;char name[64];{Lock l;s=status;enabled=saved.enabled;key=saved.key[0]!=0;memcpy(name,saved.name,sizeof(name));}
    cJSON_AddBoolToObject(out,"enabled",enabled);cJSON_AddBoolToObject(out,"key_saved",key);cJSON_AddStringToObject(out,"name",name);
    cJSON_AddStringToObject(out,"phase",s.phase);cJSON_AddBoolToObject(out,"running",s.running);cJSON_AddBoolToObject(out,"connected",s.connected);cJSON_AddBoolToObject(out,"policy_ready",s.authority);
    cJSON_AddStringToObject(out,"connection_stage",s.stage);cJSON_AddStringToObject(out,"last_error",s.last_error);cJSON_AddNumberToObject(out,"error_code",s.error_code);cJSON_AddNumberToObject(out,"error_detail",s.error_detail);
    cJSON_AddNumberToObject(out,"peers",s.peers);cJSON_AddNumberToObject(out,"max_peers",CONFIG_ML_MAX_PEERS);
    cJSON_AddNumberToObject(out,"panel_packets_received",accepted_packets.load());cJSON_AddNumberToObject(out,"panel_packets_rejected",rejected_packets.load());
    char ip[16]{};if(s.ip)microlink_ip_to_str(s.ip,ip);cJSON_AddStringToObject(out,"ip",ip);
    cJSON_AddNumberToObject(out,"internal_free_bytes",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    if(s.minimum_internal!=UINT32_MAX)cJSON_AddNumberToObject(out,"vpn_minimum_internal_bytes",s.minimum_internal);
    cJSON_AddNumberToObject(out,"psram_free_bytes",heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    cJSON_AddBoolToObject(out,"subnet_routing",false);return out;
}
}

extern "C" void ecomax_tailnet_coord(const char *stage,int code,int detail){tailnet::coord(stage,code,detail);}
