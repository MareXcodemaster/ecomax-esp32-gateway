#pragma once
#include "cJSON.h"
#include <array>
#include <cmath>
#include <ctime>
namespace tapo_energy {
inline const cJSON *field(const cJSON *o,const char *key){return cJSON_GetObjectItemCaseSensitive(o,key);}
inline double number(const cJSON *o,const char *key,double maximum){auto *v=field(o,key);return cJSON_IsNumber(v)&&std::isfinite(v->valuedouble)&&v->valuedouble>=0&&v->valuedouble<=maximum?v->valuedouble:NAN;}
struct Usage {double power=NAN,today=NAN,month=NAN;};
inline Usage usage(const cJSON *o){return {number(o,"current_power",100000000)/1000,number(o,"today_energy",2400000)/1000,number(o,"month_energy",100000000)/1000};}
struct Days {std::array<time_t,8> start{};std::array<double,7> kwh{{NAN,NAN,NAN,NAN,NAN,NAN,NAN}};};
inline void local(time_t epoch,tm &date){
#ifdef _WIN32
    localtime_s(&date,&epoch);
#else
    localtime_r(&epoch,&date);
#endif
}
inline bool range(time_t current,Days &days){
    if(current<1735689600||current>4102444800)return false;
    tm today{};local(current,today);today.tm_hour=today.tm_min=today.tm_sec=0;
    for(unsigned i=0;i<8;i++){tm date=today;date.tm_mday+=static_cast<int>(i)-6;date.tm_isdst=-1;days.start[i]=mktime(&date);if(i&&days.start[i]<=days.start[i-1])return false;}
    return true;
}
inline bool hourly(const cJSON *o,Days &days){
    auto *values=field(o,"data");auto start=number(o,"start_timestamp",4102444800),interval=number(o,"interval",1440);
    if(!cJSON_IsArray(values)||start!=days.start[0]||interval!=60||cJSON_GetArraySize(values)>175)return false;
    std::array<unsigned,7> count{};std::array<double,7> wh{};std::array<bool,7> valid{{true,true,true,true,true,true,true}};
    for(int i=0;i<cJSON_GetArraySize(values);i++){
        time_t epoch=days.start[0]+static_cast<time_t>(i)*3600;
        unsigned day=0;while(day<7&&epoch>=days.start[day+1])++day;if(day>=7)break;
        auto *v=cJSON_GetArrayItem(values,i);++count[day];
        if(!cJSON_IsNumber(v)||!std::isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>100000||std::floor(v->valuedouble)!=v->valuedouble)valid[day]=false;
        else wh[day]+=v->valuedouble;
    }
    for(unsigned day=0;day<6;day++)if(valid[day]&&count[day]==static_cast<unsigned>((days.start[day+1]-days.start[day])/3600))days.kwh[day]=wh[day]/1000;
    return true;
}
inline double week(const Days &days,double today){double total=today;for(unsigned day=0;day<6;day++)total+=days.kwh[day];return total;}
}
