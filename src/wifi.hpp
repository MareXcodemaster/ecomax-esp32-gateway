#pragma once
#include "esp_err.h"
#include "cJSON.h"
namespace network {
void start();
esp_err_t configure(const char *ssid, const char *password);
void request_scan();
cJSON *scan_results();
}

