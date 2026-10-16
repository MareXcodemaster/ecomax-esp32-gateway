#pragma once
#include "esp_err.h"
#include <cstddef>
using nvs_handle_t = unsigned;
constexpr int NVS_READWRITE = 1;
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
esp_err_t nvs_open(const char*, int, nvs_handle_t*);
esp_err_t nvs_get_str(nvs_handle_t, const char*, char*, size_t*);
esp_err_t nvs_set_str(nvs_handle_t, const char*, const char*);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
