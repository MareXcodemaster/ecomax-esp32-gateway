#pragma once
#include "cJSON.h"
#include <string>
namespace cwu_pump {
void start();
bool self_test();
cJSON *snapshot();
bool configure(const cJSON *request,std::string &error);
}
