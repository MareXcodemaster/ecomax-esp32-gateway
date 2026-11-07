#pragma once
#include "cJSON.h"
#include <string>
namespace tailnet {
void init();
void activate();
void tick();
cJSON *snapshot();
bool configure(const cJSON *body,std::string &error);
}
