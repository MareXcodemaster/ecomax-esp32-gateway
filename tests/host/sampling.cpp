#include "sampling.hpp"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
namespace { std::string stored,pending;bool failure=false;unsigned writes=0; }
esp_err_t nvs_open(const char *name,int,nvs_handle_t *h){assert(!strcmp(name,"sampling"));*h=2;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t,const char *key,void *out,size_t *size){assert(!strcmp(key,"intervals"));if(stored.empty())return ESP_ERR_NVS_NOT_FOUND;if(*size<stored.size())return ESP_FAIL;memcpy(out,stored.data(),stored.size());*size=stored.size();return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t,const char *key,const void *data,size_t size){assert(!strcmp(key,"intervals"));pending.assign(static_cast<const char *>(data),size);++writes;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t){if(failure)return ESP_FAIL;stored=pending;return ESP_OK;}
void nvs_close(nvs_handle_t){}
bool configure(const char *json){auto *body=cJSON_Parse(json);std::string error;bool ok=sampling::configure(body,error);cJSON_Delete(body);return ok;}
int main(){
 sampling::init();assert(sampling::boiler_ms()==3000&&sampling::panel_ms()==2000&&sampling::history_ms()==60000);
 assert(configure("{\"boiler_ms\":1000,\"panel_ms\":500,\"history_ms\":30000}"));
 assert(sampling::boiler_ms()==1000&&sampling::panel_ms()==500&&sampling::history_ms()==30000);sampling::init();assert(sampling::history_ms()==30000);
 const auto previous=writes;
 for(const char *s:{"{}","null","{\"boiler_ms\":0,\"panel_ms\":500,\"history_ms\":30000}","{\"boiler_ms\":1100.5,\"panel_ms\":500,\"history_ms\":30000}","{\"boiler_ms\":1000,\"panel_ms\":\"500\",\"history_ms\":30000}","{\"boiler_ms\":1000,\"panel_ms\":500,\"history_ms\":600001}","{\"boiler_ms\":1e100,\"panel_ms\":500,\"history_ms\":30000}"})assert(!configure(s));
 assert(writes==previous&&sampling::boiler_ms()==1000);
 assert(configure("{\"boiler_ms\":1000,\"panel_ms\":500,\"history_ms\":30000}")&&writes==previous);
 failure=true;assert(!configure("{\"boiler_ms\":10000,\"panel_ms\":10000,\"history_ms\":600000}"));assert(sampling::panel_ms()==500);sampling::init();assert(sampling::panel_ms()==500);
 failure=false;assert(configure("{\"boiler_ms\":10000,\"panel_ms\":10000,\"history_ms\":600000}"));sampling::init();assert(sampling::history_ms()==600000);
 auto *snapshot=sampling::snapshot();assert(cJSON_GetObjectItemCaseSensitive(snapshot,"panel_ms")->valueint==10000);assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(snapshot,"defaults"),"history_ms")->valueint==60000);cJSON_Delete(snapshot);
 stored="corrupt";sampling::init();assert(sampling::boiler_ms()==3000&&sampling::history_ms()==60000);
 puts("PASS sampling: defaults, limits, malformed input, NVS persistence, failed commit, no redundant writes");
}
