#pragma once
#include "cJSON.h"
#include <string>
namespace panel {
void start();
bool is_busy();
cJSON *snapshot();
cJSON *job_snapshot(const char *id);
cJSON *submit(const char *kind, const cJSON *payload, const char *owner, std::string &error);
bool self_test();
void set_clock(double seconds);
}
