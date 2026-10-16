#include "device_access.hpp"
#include "bootloader_random.h"
#include "esp_random.h"
#include "nvs.h"
#include "mbedtls/platform_util.h"
#include <cstring>
#include <cstdint>

namespace device_access {
static char secret[33]{};
static bool valid() {
    if (strlen(secret) != 32) return false;
    for (unsigned i=0;i<32;++i)
        if (!((secret[i]>='0'&&secret[i]<='9')||(secret[i]>='a'&&secret[i]<='f'))) return false;
    return true;
}
esp_err_t init() {
    mbedtls_platform_zeroize(secret,sizeof(secret));
    nvs_handle_t handle;
    auto result=nvs_open("device_access",NVS_READWRITE,&handle);
    if (result!=ESP_OK) return result;
    size_t length=sizeof(secret);
    result=nvs_get_str(handle,"secret",secret,&length);
    if (result==ESP_OK) {
        secret[32]=0;
        if (!valid()) result=ESP_ERR_INVALID_STATE;
    } else if (result==ESP_ERR_NVS_NOT_FOUND) {
        uint8_t bytes[16];
        // Called before Wi-Fi/ADC init; disable the temporary entropy source afterwards.
        bootloader_random_enable();
        esp_fill_random(bytes,sizeof(bytes));
        bootloader_random_disable();
        constexpr char hex[]="0123456789abcdef";
        for (unsigned i=0;i<sizeof(bytes);++i) {secret[2*i]=hex[bytes[i]>>4];secret[2*i+1]=hex[bytes[i]&15];}
        secret[32]=0;
        mbedtls_platform_zeroize(bytes,sizeof(bytes));
        result=nvs_set_str(handle,"secret",secret);
        if (result==ESP_OK) result=nvs_commit(handle);
    }
    nvs_close(handle);
    if (result!=ESP_OK) mbedtls_platform_zeroize(secret,sizeof(secret));
    return result;
}
const char *password() {return secret;}
}
