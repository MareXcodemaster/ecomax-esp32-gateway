#include "device_info.hpp"
#include "history_store.hpp"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "esp_private/esp_clk.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <cstdio>

namespace device_info {
namespace {
struct Hardware {
    esp_chip_info_t chip{};
    uint32_t flash=0,image=0,slot=0;
    bool flash_ok=false,image_ok=false;
    char partition[17]{};
};
Hardware hardware_info(){
    Hardware value;esp_chip_info(&value.chip);
    value.flash_ok=esp_flash_get_size(nullptr,&value.flash)==ESP_OK;
    if(const auto *running=esp_ota_get_running_partition()){
        value.slot=running->size;snprintf(value.partition,sizeof(value.partition),"%s",running->label);
        esp_partition_pos_t position{running->address,running->size};esp_image_metadata_t metadata{};
        // Read segment metadata once, not the entire flash on each panel refresh.
        if(esp_image_get_metadata(&position,&metadata)==ESP_OK&&metadata.image_len&&metadata.image_len<=value.slot){
            value.image=metadata.image_len;value.image_ok=true;
        }
    }
    return value;
}
cJSON *memory(uint32_t caps){
    multi_heap_info_t info{};heap_caps_get_info(&info,caps);
    auto *out=cJSON_CreateObject();
    if(!out)return nullptr;
    cJSON_AddNumberToObject(out,"total_bytes",info.total_allocated_bytes+info.total_free_bytes);
    cJSON_AddNumberToObject(out,"used_bytes",info.total_allocated_bytes);
    cJSON_AddNumberToObject(out,"free_bytes",info.total_free_bytes);
    cJSON_AddNumberToObject(out,"minimum_free_bytes",info.minimum_free_bytes);
    cJSON_AddNumberToObject(out,"largest_free_block_bytes",info.largest_free_block);
    return out;
}
}
cJSON *snapshot(){
    static const Hardware hardware=hardware_info();
    auto *out=cJSON_CreateObject();
    if(!out)return nullptr;
    cJSON_AddNumberToObject(out,"schema",1);
    cJSON_AddStringToObject(out,"chip",hardware.chip.model==CHIP_ESP32S3?"ESP32-S3":"ESP32");
    cJSON_AddNumberToObject(out,"chip_revision",hardware.chip.revision);
    cJSON_AddNumberToObject(out,"cpu_cores",hardware.chip.cores);
    cJSON_AddNumberToObject(out,"cpu_mhz",esp_clk_cpu_freq()/1000000);
    cJSON_AddStringToObject(out,"idf_version",esp_get_idf_version());
    cJSON_AddNumberToObject(out,"uptime_ms",esp_timer_get_time()/1000);
    cJSON_AddNumberToObject(out,"reset_reason",esp_reset_reason());
    if(hardware.flash_ok)cJSON_AddNumberToObject(out,"flash_total_bytes",hardware.flash);
    else cJSON_AddNullToObject(out,"flash_total_bytes");
    auto *firmware=cJSON_AddObjectToObject(out,"firmware");
    cJSON_AddStringToObject(firmware,"partition",hardware.partition);
    cJSON_AddNumberToObject(firmware,"total_bytes",hardware.slot);
    if(hardware.image_ok){
        cJSON_AddNumberToObject(firmware,"used_bytes",hardware.image);
        cJSON_AddNumberToObject(firmware,"free_bytes",hardware.slot-hardware.image);
    }else{
        cJSON_AddNullToObject(firmware,"used_bytes");cJSON_AddNullToObject(firmware,"free_bytes");
    }
    cJSON_AddItemToObject(out,"internal_ram",memory(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    cJSON_AddItemToObject(out,"psram",memory(MALLOC_CAP_SPIRAM));
    cJSON_AddItemToObject(out,"history",history_store::status());
    return out;
}
}
