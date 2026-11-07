#pragma once
#include "cJSON.h"
#include <cstdint>
namespace runtime_stats {
enum class Task : unsigned { main,rs485,panel,http,wifi,dns,usb,updates,history,tapo,tailnet,count };
void sample(Task id,uint32_t stack_bytes);
void stopped(Task id);
cJSON *snapshot();
}
