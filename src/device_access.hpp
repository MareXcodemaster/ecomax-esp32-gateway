#pragma once
#include "esp_err.h"
namespace device_access {
esp_err_t init();
const char *password();
}
