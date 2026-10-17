#pragma once
#include "esp_err.h"
namespace device_access {
esp_err_t init();
bool valid_password(const char *value,unsigned minimum);
esp_err_t configure(const char *current,const char *web,const char *ap=nullptr);
const char *password();
const char *setup_password();
}
