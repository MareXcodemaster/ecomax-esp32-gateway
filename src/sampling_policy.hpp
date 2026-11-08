#pragma once
#include "cJSON.h"
#include <cmath>
#include <cstdint>
namespace sampling_policy {
struct Config { uint32_t boiler_ms=3000,panel_ms=2000,history_ms=60000; };
inline bool range(double n,uint32_t low,uint32_t high,uint32_t step){return std::isfinite(n)&&n>=low&&n<=high&&std::floor(n)==n&&static_cast<uint32_t>(n)%step==0;}
inline bool valid(const Config &c){return range(c.boiler_ms,1000,10000,100)&&range(c.panel_ms,500,10000,100)&&range(c.history_ms,30000,600000,1000);}
inline bool parse(const cJSON *body,Config &out){
    auto *a=cJSON_GetObjectItemCaseSensitive(body,"boiler_ms"),*b=cJSON_GetObjectItemCaseSensitive(body,"panel_ms"),*c=cJSON_GetObjectItemCaseSensitive(body,"history_ms");
    if(!cJSON_IsNumber(a)||!cJSON_IsNumber(b)||!cJSON_IsNumber(c)||!range(a->valuedouble,1000,10000,100)||!range(b->valuedouble,500,10000,100)||!range(c->valuedouble,30000,600000,1000))return false;
    out={static_cast<uint32_t>(a->valuedouble),static_cast<uint32_t>(b->valuedouble),static_cast<uint32_t>(c->valuedouble)};return true;
}
}
