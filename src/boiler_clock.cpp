#include "boiler_clock.hpp"
#include "boiler_clock_codec.hpp"
#include "transport.hpp"
#include "wifi.hpp"
#include "state.hpp"
#include "updates.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <cmath>
#include <cstdio>
#include <cstring>
namespace boiler_clock {
namespace {
SemaphoreHandle_t mutex;
struct Lock{Lock(){xSemaphoreTake(mutex,portMAX_DELAY);}~Lock(){xSemaphoreGive(mutex);}};
bool automatic=false,supported=false,available=false,queued=false,waiting=false,verification_failed=false;
uint64_t next_check=0,last_attempt=0,sent_ms=0,last_verified=0;
int64_t target_civil=0;
const char *message="Brak synchronizacji";
uint64_t now(){return esp_timer_get_time()/1000;}
bool sample(uint8_t p[26],uint64_t &received){return transport::clock_sample(p,received)&&now()>=received&&now()-received<10000;}
bool local_clock(tm &local){time_t epoch=time(nullptr);return epoch>=1704067200&&epoch<=4102444800&&localtime_r(&epoch,&local);}
}
void init(){
    mutex=xSemaphoreCreateMutex();configASSERT(mutex);nvs_handle_t h;
    if(nvs_open("boiler_clock",NVS_READONLY,&h)==ESP_OK){uint8_t value=0;if(nvs_get_u8(h,"automatic",&value)==ESP_OK)automatic=value==1;nvs_close(h);}
    network::continuous_time(automatic);
}
bool configure(const cJSON *body,std::string &error){
    Lock lock;auto *setting=cJSON_GetObjectItemCaseSensitive(body,"automatic");auto *action=cJSON_GetObjectItemCaseSensitive(body,"action");
    if(cJSON_IsBool(setting)&&!action){
        bool enable=cJSON_IsTrue(setting);if(enable&&!supported){error="Synchronizacja tego modelu nie jest jeszcze potwierdzona";return false;}
        nvs_handle_t h;auto result=nvs_open("boiler_clock",NVS_READWRITE,&h);
        if(result==ESP_OK){result=nvs_set_u8(h,"automatic",enable);if(result==ESP_OK)result=nvs_commit(h);nvs_close(h);}
        if(result!=ESP_OK){error="Nie zapisano ustawienia synchronizacji";return false;}
        automatic=enable;verification_failed=false;network::continuous_time(automatic);next_check=0;return true;
    }
    if(!setting&&cJSON_IsString(action)&&!strcmp(action->valuestring,"sync")){
        if(!available||!supported){error="Brak gotowego polaczenia RS485 dla synchronizacji";return false;}
        if(queued||waiting){error="Synchronizacja juz trwa";return false;}
        if(last_attempt&&now()-last_attempt<30000){error="Poczekaj 30 sekund od poprzedniej proby";return false;}
        queued=true;verification_failed=false;message="Oczekiwanie na synchronizacje";return true;
    }
    error="Nieprawidlowe ustawienie zegara";return false;
}
void tick(bool compatible,bool connected){
    uint8_t p[26]{},payload[26]{};uint64_t received=0;bool send=false;
    {
        Lock lock;supported=compatible;available=compatible&&connected;
        const auto tick=now();bool fresh=sample(p,received);
        if(waiting){
            if(fresh&&received>sent_ms&&std::llabs(boiler_clock_codec::civil(p)-(target_civil+static_cast<int64_t>((received-sent_ms)/1000)))<=3){
                waiting=false;last_verified=time(nullptr);message="Zsynchronizowano; potwierdzone przez panel kotla";state::log("Boiler clock: main-panel readback verified");
            }else if(tick-sent_ms>20000){waiting=false;verification_failed=true;message="Brak potwierdzenia zmiany czasu; automat wstrzymany do ponownego wlaczenia";}
            return;
        }
        if(!queued&&(!automatic||tick<next_check))return;
        if(verification_failed&&!queued)return;
        if(updates::installing())return;
        bool manual=queued;queued=false;next_check=tick+30000;
        if(!available||!fresh){if(manual)message="Brak swiezego odczytu zegara RS485";return;}
        if(!manual&&!network::time_synchronized()){message="Oczekiwanie na czas z internetu (NTP)";return;}
        tm local{};if(!local_clock(local)){message="Brak aktualnego czasu ESP";return;}
        int64_t difference=boiler_clock_codec::civil(local)-boiler_clock_codec::civil(p)-static_cast<int64_t>((tick-received)/1000);
        if(std::llabs(difference)<=10){message="Zegar kotla jest zgodny";return;}
        if(!boiler_clock_codec::encode(p,sizeof(p),local,payload)){message="Daty roznia sie; ustaw poprawna date na ekranie kotla";return;}
        if(std::llabs(difference)>7200){message="Roznica przekracza 2 godziny; sprawdz zegar kotla";return;}
        if(!manual&&last_attempt&&tick-last_attempt<300000)return;
        last_attempt=tick;sent_ms=tick;target_civil=boiler_clock_codec::civil(payload);waiting=true;
        message="Wysylanie czasu; oczekiwanie na panel kotla";send=true;
    }
    if(send){
        transport::Reply reply;bool ok=transport::set_clock(payload,sizeof(payload),reply);
        Lock lock;
        if(!ok){waiting=false;verification_failed=true;message="Brak potwierdzenia wyslania; automat wstrzymany. Sprawdz RS485";}
        state::log("Boiler clock broadcast: sent=%u, awaiting independent panel readback",reply.sent);
    }
}
cJSON *snapshot(){
    Lock lock;auto *out=cJSON_CreateObject();if(!out)return nullptr;
    uint8_t p[26]{};uint64_t received=0;bool present=transport::clock_sample(p,received);auto tick=now();bool fresh=present&&tick>=received&&tick-received<10000;
    cJSON_AddBoolToObject(out,"fresh",fresh);cJSON_AddBoolToObject(out,"supported",supported);cJSON_AddBoolToObject(out,"available",available&&fresh);
    cJSON_AddBoolToObject(out,"automatic",automatic);cJSON_AddBoolToObject(out,"pending",queued||waiting);
    cJSON_AddBoolToObject(out,"ntp_synchronized",network::time_synchronized());cJSON_AddStringToObject(out,"timezone","Europe/Warsaw");
    cJSON_AddStringToObject(out,"message",supported?message:"Odczyt czasu; synchronizacja tego modelu nie jest jeszcze potwierdzona");
    cJSON_AddNumberToObject(out,"last_verified_epoch",last_verified);
    if(present){char value[32];snprintf(value,sizeof(value),"%04u-%02u-%02u %02u:%02u:%02u",p[7]|(p[8]<<8),p[9],p[10],p[4],p[5],p[6]);
        cJSON_AddStringToObject(out,"device_time",value);cJSON_AddNumberToObject(out,"age_ms",tick-received);
        tm local{};if(fresh&&local_clock(local))cJSON_AddNumberToObject(out,"difference_seconds",boiler_clock_codec::civil(local)-boiler_clock_codec::civil(p)-static_cast<int64_t>((tick-received)/1000));
    }
    return out;
}
}
