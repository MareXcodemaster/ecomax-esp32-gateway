#pragma once
#include "cJSON.h"
#include <string>
namespace panel {
void start();
void activate();
bool is_busy();
cJSON *snapshot(bool compact=false,const char *catalog=nullptr);
cJSON *compatibility_snapshot(const char *id);
cJSON *job_snapshot(const char *id);
cJSON *submit(const char *kind, const cJSON *payload, const char *owner, std::string &error);
bool self_test();
void set_clock(double seconds);
}
