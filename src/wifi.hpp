#pragma once
#include "esp_err.h"
#include "cJSON.h"
namespace network {
void start();
esp_err_t configure(const char *ssid, const char *password);
esp_err_t configure_hostname(const char *hostname);
void request_scan();
cJSON *scan_results();
}

