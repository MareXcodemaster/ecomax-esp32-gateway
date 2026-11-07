#pragma once
#include "cJSON.h"
#include <string>
namespace boiler_clock {
void init();
void tick(bool supported,bool available);
cJSON *snapshot();
bool configure(const cJSON *body,std::string &error);
}
