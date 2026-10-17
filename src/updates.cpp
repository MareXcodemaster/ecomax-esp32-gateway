#include "updates.hpp"
#include "update_manifest.hpp"
#include "board.hpp"
#include "state.hpp"
#include "panel.hpp"
#include "pellet.hpp"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_netif_sntp.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <atomic>
#include <algorithm>
#include <ctime>
#include <strings.h>

namespace updates {
using update_manifest::Candidate;
static SemaphoreHandle_t mutex;
static std::atomic<bool> flash_busy{false},check_pending{false},install_pending{false},worker_ready{false};
static bool enabled=true,include_beta=true,working=false,available=false;
static uint64_t checked=0; static size_t received=0;
static char phase[32]="starting",error[160]{},requested[32]{};
static Candidate candidate;
static uint64_t millis(){return esp_timer_get_time()/1000;}
bool installing(){return flash_busy.load();}
bool reserve_flash(){bool expected=false;return flash_busy.compare_exchange_strong(expected,true);}
void release_flash(){flash_busy.store(false);}
static void status(const char *next,const char *message="") {
    xSemaphoreTake(mutex,portMAX_DELAY);
    snprintf(phase,sizeof(phase),"%s",next);snprintf(error,sizeof(error),"%s",message);
    xSemaphoreGive(mutex);
}
struct Stream {
    esp_http_client_handle_t client=nullptr; char location[2048]{};
    ~Stream(){close();}
    void close(){if(client){esp_http_client_close(client);esp_http_client_cleanup(client);client=nullptr;}}
    static esp_err_t event(esp_http_client_event_t *event){
        if(event->event_id==HTTP_EVENT_ON_HEADER&&!strcasecmp(event->header_key,"Location")){
            auto *self=static_cast<Stream *>(event->user_data);
            if(strlen(event->header_value)>=sizeof(self->location))return ESP_FAIL;
            snprintf(self->location,sizeof(self->location),"%s",event->header_value);
        }return ESP_OK;
    }
    int open(const char *url){
        close();location[0]=0;
        esp_http_client_config_t config{};config.url=url;config.timeout_ms=15000;
        config.crt_bundle_attach=esp_crt_bundle_attach;config.disable_auto_redirect=true;
        config.event_handler=event;config.user_data=this;config.user_agent="ecoMAX-WiFi-updater";
        config.buffer_size=2048;config.buffer_size_tx=1024;
        client=esp_http_client_init(&config);if(!client)return -1;
        esp_http_client_set_header(client,"Connection","close");
        esp_http_client_set_header(client,"Accept","application/vnd.github+json");
        if(esp_http_client_open(client,0)!=ESP_OK)return -1;
        if(esp_http_client_fetch_headers(client)<0)return -1;
        return esp_http_client_get_status_code(client);
    }
};
static int latest(Candidate &value,bool beta){
    Stream stream;int code=stream.open(beta?"https://api.github.com/repos/MareXcodemaster/ecomax-esp32-gateway/releases?per_page=5":"https://api.github.com/repos/MareXcodemaster/ecomax-esp32-gateway/releases/latest");
    if(code==404){value={};return 404;}
    if(code!=200)return code;
    constexpr size_t limit=196608;
    auto *body=static_cast<char *>(heap_caps_malloc(limit+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!body)return -1;
    size_t used=0;bool ok=true;uint64_t deadline=millis()+30000;
    while(used<limit&&millis()<deadline){
        int count=esp_http_client_read(stream.client,body+used,std::min<size_t>(2048,limit-used));
        if(count<0){ok=false;break;}if(count==0)break;used+=count;vTaskDelay(1);
    }
    if(used==limit||millis()>=deadline||!esp_http_client_is_complete_data_received(stream.client))ok=false;
    body[used]=0;auto *root=ok?cJSON_ParseWithLength(body,used+1):nullptr;
    bool valid=root&&(beta?cJSON_IsArray(root):cJSON_IsObject(root));
    ok=valid&&(beta?update_manifest::select(root,value,true):update_manifest::parse(root,value));cJSON_Delete(root);heap_caps_free(body);
    return ok?200:valid&&beta?404:-2;
}
static bool refresh(){
    xSemaphoreTake(mutex,portMAX_DELAY);bool beta=include_beta;xSemaphoreGive(mutex);
    Candidate value;status("checking");int code=latest(value,beta);
    xSemaphoreTake(mutex,portMAX_DELAY);
    available=false;candidate={};checked=static_cast<uint64_t>(time(nullptr));
    if(code==200){candidate=value;available=update_manifest::newer(value.version,board::version);}
    xSemaphoreGive(mutex);
    if(code==404){status("no_release");return true;}
    if(code!=200){status("error",code==403||code==429?"Limit zapytan GitHub. Sprobuj pozniej.":code==-2?"Nieprawidlowe metadane wydania.":"Nie mozna sprawdzic GitHuba przez HTTPS. Kociol dziala niezaleznie.");return false;}
    status(available?"available":"up_to_date");return true;
}
static bool download(const Candidate &value){
    Stream stream;std::string url=value.url;int code=-1;
    for(unsigned hop=0;hop<4;++hop){
        if(!update_manifest::download_url(url.c_str()))return false;
        code=stream.open(url.c_str());
        if(code==200)break;
        if(code!=301&&code!=302&&code!=303&&code!=307&&code!=308)return false;
        url=stream.location;
    }
    if(code!=200)return false;
    auto length=esp_http_client_get_content_length(stream.client);
    if(length>0&&static_cast<size_t>(length)!=value.size)return false;
    const auto *partition=esp_ota_get_next_update_partition(nullptr);
    if(!partition||value.size>partition->size)return false;
    esp_ota_handle_t ota;
    if(esp_ota_begin(partition,value.size,&ota)!=ESP_OK)return false;
    mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);
    bool ok=mbedtls_sha256_starts(&hash,0)==0;
    uint8_t buffer[2048];size_t total=0;uint64_t deadline=millis()+180000;
    while(ok&&total<value.size&&millis()<deadline){
        int count=esp_http_client_read(stream.client,reinterpret_cast<char *>(buffer),std::min(sizeof(buffer),value.size-total));
        if(count<=0){ok=false;break;}
        ok=mbedtls_sha256_update(&hash,buffer,count)==0&&esp_ota_write(ota,buffer,count)==ESP_OK;total+=count;
        xSemaphoreTake(mutex,portMAX_DELAY);received=total;xSemaphoreGive(mutex);vTaskDelay(1);
    }
    uint8_t digest[32];char hex[65]{};
    ok=ok&&total==value.size&&mbedtls_sha256_finish(&hash,digest)==0;
    mbedtls_sha256_free(&hash);
    if(ok)for(unsigned i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",digest[i]);
    ok=ok&&!strcmp(hex,value.sha256);
    if(!ok){esp_ota_abort(ota);return false;}
    if(esp_ota_end(ota)!=ESP_OK)return false;
    esp_app_desc_t description{};
    if(esp_ota_get_partition_description(partition,&description)!=ESP_OK||
       strcmp(description.project_name,esp_app_get_description()->project_name)||strcmp(description.version,value.version))return false;
    if(esp_ota_set_boot_partition(partition)!=ESP_OK)return false;
    pellet::checkpoint(true);status("restarting");vTaskDelay(pdMS_TO_TICKS(2000));esp_restart();return true;
}
esp_err_t check(){
    if(!mutex||!worker_ready.load()||installing())return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex,portMAX_DELAY);bool busy=working; xSemaphoreGive(mutex);
    if(busy)return ESP_ERR_INVALID_STATE;
    check_pending.store(true);return ESP_OK;
}
esp_err_t configure(bool value,bool beta){
    if(!mutex)return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(working||installing()){xSemaphoreGive(mutex);return ESP_ERR_INVALID_STATE;}
    nvs_handle_t handle;auto result=nvs_open("lazar_s0",NVS_READWRITE,&handle);
    if(result==ESP_OK){
        result=nvs_set_u8(handle,"update_auto",value?1:0);
        if(result==ESP_OK)result=nvs_set_u8(handle,"update_beta",beta?1:0);
        if(result==ESP_OK)result=nvs_commit(handle);
        nvs_close(handle);
    }
    if(result==ESP_OK){enabled=value;if(include_beta!=beta){include_beta=beta;candidate={};available=false;check_pending.store(true);}}
    xSemaphoreGive(mutex);return result;
}
esp_err_t install(const char *version){
    if(!mutex||!worker_ready.load()||!board::github_ota||!version||panel::is_busy()||time(nullptr)<1704067200)return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex,portMAX_DELAY);
    bool valid=!working&&available&&!strcmp(version,candidate.version)&&reserve_flash();
    if(valid){working=true;received=0;snprintf(requested,sizeof(requested),"%s",version);install_pending.store(true);}
    xSemaphoreGive(mutex);return valid?ESP_OK:ESP_ERR_INVALID_STATE;
}
cJSON *snapshot(){
    auto *out=cJSON_CreateObject();cJSON_AddStringToObject(out,"current",board::version);
    cJSON_AddBoolToObject(out,"install_supported",board::github_ota);
    if(!mutex){cJSON_AddStringToObject(out,"phase","error");cJSON_AddStringToObject(out,"error","Aktualizator niedostepny: brak pamieci.");return out;}
    xSemaphoreTake(mutex,portMAX_DELAY);
    cJSON_AddStringToObject(out,"channel",include_beta?"all":"stable");cJSON_AddBoolToObject(out,"beta",candidate.beta);
    cJSON_AddBoolToObject(out,"automatic",enabled);cJSON_AddBoolToObject(out,"available",available);
    cJSON_AddBoolToObject(out,"busy",working||check_pending.load()||!worker_ready.load());cJSON_AddStringToObject(out,"phase",phase);
    cJSON_AddStringToObject(out,"error",error);cJSON_AddStringToObject(out,"version",candidate.version);
    cJSON_AddNumberToObject(out,"checked",checked);cJSON_AddNumberToObject(out,"received",received);cJSON_AddNumberToObject(out,"size",candidate.size);
    xSemaphoreGive(mutex);return out;
}
static void task(void *){
    uint64_t next=millis()+20000;bool time_started=false;
    for(;;){
        bool download_pending=install_pending.exchange(false);
        xSemaphoreTake(mutex,portMAX_DELAY);bool auto_check=enabled,busy=working;xSemaphoreGive(mutex);
        if(time_started&&(time(nullptr)>=1704067200||(!auto_check&&!check_pending.load()&&!download_pending))){
            esp_netif_sntp_deinit();time_started=false;
        }
        bool wanted=download_pending||check_pending.load()||(auto_check&&millis()>=next&&!busy&&!installing());
        if(wanted&&state::network().connected){
            if(time(nullptr)<1704067200){
                if(!time_started){esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");time_started=esp_netif_sntp_init(&config)==ESP_OK;}
                status("waiting_for_time");
                if(download_pending){release_flash();xSemaphoreTake(mutex,portMAX_DELAY);working=false;xSemaphoreGive(mutex);}
            }else{
                check_pending.store(false);xSemaphoreTake(mutex,portMAX_DELAY);working=true;xSemaphoreGive(mutex);
                bool ok=refresh();next=millis()+(ok?86400000ULL:3600000ULL);
                if(download_pending){
                    xSemaphoreTake(mutex,portMAX_DELAY);Candidate value=candidate; xSemaphoreGive(mutex);
                    if(ok&&available&&!strcmp(value.version,requested)&&board::github_ota){status("downloading");ok=download(value);}
                    else ok=false;
                    if(!ok)status("error","Aktualizacja nie zostala wgrana. Sprawdz polaczenie, zgodnosc obrazu i nowa wersje. Nie ponowiono automatycznie.");
                    release_flash();
                }
                xSemaphoreTake(mutex,portMAX_DELAY);working=false;xSemaphoreGive(mutex);
            }
        }else if(download_pending){status("error","Brak Wi-Fi. Nie rozpoczeto aktualizacji.");release_flash();xSemaphoreTake(mutex,portMAX_DELAY);working=false;xSemaphoreGive(mutex);}
        else if(wanted)status("waiting_for_wifi");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void init(){
    mutex=xSemaphoreCreateMutex();if(!mutex){state::log("Optional GitHub updater unavailable: memory");return;}
    nvs_handle_t handle;if(nvs_open("lazar_s0",NVS_READONLY,&handle)==ESP_OK){uint8_t value;if(nvs_get_u8(handle,"update_auto",&value)==ESP_OK)enabled=value!=0;if(nvs_get_u8(handle,"update_beta",&value)==ESP_OK)include_beta=value!=0;nvs_close(handle);}
}
void start(){
    if(!mutex)return;
    worker_ready.store(xTaskCreate(task,"github_updates",12288,nullptr,1,nullptr)==pdPASS);
    if(worker_ready.load())status("idle");
    if(!worker_ready.load()){status("error","Aktualizator niedostepny: brak pamieci zadania.");state::log("Optional GitHub updater unavailable: task");}
}
}
