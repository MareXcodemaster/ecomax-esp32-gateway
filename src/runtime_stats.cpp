#include "runtime_stats.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
namespace runtime_stats {
namespace {
struct Entry { uint32_t bytes=0,minimum=UINT32_MAX;bool running=false; };
constexpr unsigned count=static_cast<unsigned>(Task::count);
Entry entries[count];portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
const char *names[]={"main","rs485_rx","panel","http","wifi_manager","setup_dns","usb_config","github_updates","history_flash","cwu_tapo"};
}
void sample(Task id,uint32_t bytes){
    const auto free=uxTaskGetStackHighWaterMark(nullptr);const auto i=static_cast<unsigned>(id);
    portENTER_CRITICAL(&mux);auto &v=entries[i];v.bytes=bytes;v.minimum=std::min(v.minimum,static_cast<uint32_t>(free));v.running=true;portEXIT_CRITICAL(&mux);
}
void stopped(Task id){portENTER_CRITICAL(&mux);entries[static_cast<unsigned>(id)].running=false;portEXIT_CRITICAL(&mux);}
cJSON *snapshot(){
    Entry copy[count];portENTER_CRITICAL(&mux);std::copy_n(entries,count,copy);portEXIT_CRITICAL(&mux);
    auto *out=cJSON_CreateArray();
    for(unsigned i=0;i<count;++i){if(!copy[i].bytes)continue;auto *row=cJSON_CreateObject();cJSON_AddStringToObject(row,"name",names[i]);cJSON_AddNumberToObject(row,"stack_bytes",copy[i].bytes);cJSON_AddNumberToObject(row,"minimum_free_bytes",copy[i].minimum);cJSON_AddBoolToObject(row,"running",copy[i].running);cJSON_AddItemToArray(out,row);}
    return out;
}
}
