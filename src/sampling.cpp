#include "sampling.hpp"
#include "nvs.h"
#include <atomic>
namespace sampling {
namespace { std::atomic<uint32_t> boiler{3000},panel{2000},history{60000};
void apply(const sampling_policy::Config &c){boiler=c.boiler_ms;panel=c.panel_ms;history=c.history_ms;}
void fields(cJSON *out,const sampling_policy::Config &c){cJSON_AddNumberToObject(out,"boiler_ms",c.boiler_ms);cJSON_AddNumberToObject(out,"panel_ms",c.panel_ms);cJSON_AddNumberToObject(out,"history_ms",c.history_ms);}
}
void init(){apply({});nvs_handle_t h;if(nvs_open("sampling",NVS_READONLY,&h)==ESP_OK){sampling_policy::Config c;size_t size=sizeof(c);if(nvs_get_blob(h,"intervals",&c,&size)==ESP_OK&&size==sizeof(c)&&sampling_policy::valid(c))apply(c);nvs_close(h);}}
uint32_t boiler_ms(){return boiler.load();} uint32_t panel_ms(){return panel.load();} uint32_t history_ms(){return history.load();}
cJSON *snapshot(){auto *out=cJSON_CreateObject();if(!out)return nullptr;fields(out,{boiler_ms(),panel_ms(),history_ms()});auto *defaults=cJSON_CreateObject();fields(defaults,{});cJSON_AddItemToObject(out,"defaults",defaults);return out;}
bool configure(const cJSON *body,std::string &error){
    sampling_policy::Config c;if(!sampling_policy::parse(body,c)){error="Odczyt: 1000-10000 ms; strona: 500-10000 ms (krok 100 ms); archiwum: 30-600 s (krok 1 s)";return false;}
    if(c.boiler_ms==boiler_ms()&&c.panel_ms==panel_ms()&&c.history_ms==history_ms())return true;
    nvs_handle_t h;auto result=nvs_open("sampling",NVS_READWRITE,&h);
    if(result==ESP_OK){result=nvs_set_blob(h,"intervals",&c,sizeof(c));if(result==ESP_OK)result=nvs_commit(h);nvs_close(h);}
    if(result!=ESP_OK){error="Nie zapisano odstepow odczytu w ESP";return false;}apply(c);return true;
}
}
