#pragma once
#include "cJSON.h"
#include "esp_err.h"
namespace updates {
void init();
void start();
cJSON *snapshot();
esp_err_t check();
esp_err_t automatic(bool enabled);
esp_err_t install(const char *version);
bool installing();
bool reserve_flash();
void release_flash();
struct FlashGuard {
    bool acquired=reserve_flash(),keep=false;
    ~FlashGuard(){if(acquired&&!keep)release_flash();}
};
}
