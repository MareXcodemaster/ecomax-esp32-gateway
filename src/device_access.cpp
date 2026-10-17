#include "device_access.hpp"
#include "nvs.h"
#include "mbedtls/platform_util.h"
#include <cstring>
#include <cstdint>

namespace device_access {
struct Credentials { uint32_t magic; char web[49],ap[49]; };
constexpr uint32_t format=0x45434131;
static Credentials active{};
bool valid_password(const char *value,unsigned minimum){
    if(!value)return false;
    size_t size=strnlen(value,49);
    if(size<minimum||size>48)return false;
    for(size_t i=0;i<size;++i)if(static_cast<unsigned char>(value[i])<32||static_cast<unsigned char>(value[i])>126)return false;
    return true;
}
static bool valid(const Credentials &value){
    return value.magic==format&&valid_password(value.web,5)&&valid_password(value.ap,8);
}
static esp_err_t store(const Credentials &value){
    nvs_handle_t handle;
    auto result=nvs_open("device_access",NVS_READWRITE,&handle);
    if(result!=ESP_OK)return result;
    result=nvs_set_blob(handle,"credentials",&value,sizeof(value));
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);return result;
}
esp_err_t init(){
    mbedtls_platform_zeroize(&active,sizeof(active));
    Credentials value{};
    nvs_handle_t handle;
    auto result=nvs_open("device_access",NVS_READWRITE,&handle);
    if(result!=ESP_OK)return result;
    size_t length=sizeof(value);
    result=nvs_get_blob(handle,"credentials",&value,&length);
    nvs_close(handle);
    if(result==ESP_OK){
        if(length!=sizeof(value)||!valid(value))result=ESP_ERR_INVALID_STATE;
    }else if(result==ESP_ERR_NVS_NOT_FOUND){
        value.magic=format;
        std::strcpy(value.web,"admin");std::strcpy(value.ap,"piecwifi");
        result=store(value);
    }
    if(result==ESP_OK)active=value;
    mbedtls_platform_zeroize(&value,sizeof(value));
    return result;
}
esp_err_t configure(const char *current,const char *web,const char *ap){
    if(!valid(active)||!valid_password(current,5)||!valid_password(web,5)||(ap&&!valid_password(ap,8)))return ESP_ERR_INVALID_ARG;
    char supplied[49]{};std::memcpy(supplied,current,std::strlen(current));
    volatile unsigned difference=0;
    for(unsigned i=0;i<sizeof(supplied);++i)difference=difference|(static_cast<unsigned char>(supplied[i])^static_cast<unsigned char>(active.web[i]));
    mbedtls_platform_zeroize(supplied,sizeof(supplied));
    if(difference)return ESP_ERR_INVALID_ARG;
    Credentials next{};next.magic=format;
    std::strcpy(next.web,web);std::strcpy(next.ap,ap?ap:active.ap);
    auto result=store(next);
    mbedtls_platform_zeroize(&next,sizeof(next));
    // New credentials take effect together after the scheduled reboot.
    return result;
}
const char *password(){return active.web;}
const char *setup_password(){return active.ap;}
}
