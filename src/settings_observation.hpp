#pragma once
#include "cJSON.h"
#include <cmath>
#include <cstdio>
#include <string>

namespace settings_observation {
inline double number(const cJSON *object,const char *key){auto *value=cJSON_GetObjectItemCaseSensitive(object,key);return cJSON_IsNumber(value)?value->valuedouble:NAN;}
inline const char *string(const cJSON *object,const char *key){auto *value=cJSON_GetObjectItemCaseSensitive(object,key);return cJSON_IsString(value)?value->valuestring:"";}
inline bool fresh(double read,double epoch){return std::isfinite(read)&&epoch-read<=20&&read-epoch<=2;}
inline cJSON *values(const cJSON *data,bool native,double epoch,bool &menu_fresh){
    if(!std::isfinite(epoch)||epoch<1577836800||epoch>4102444800||!cJSON_IsObject(data)||!fresh(number(data,"target_read_epoch"),epoch))return nullptr;
    menu_fresh=native&&fresh(number(data,"menu_read_epoch"),epoch);
    auto *out=cJSON_CreateObject();const cJSON *row;
    if(menu_fresh)cJSON_ArrayForEach(row,cJSON_GetObjectItemCaseSensitive(data,"menu")){
        const double index=number(row,"index"),value=number(cJSON_GetObjectItemCaseSensitive(row,"metadata"),"value");
        if(std::isfinite(index)&&index>=0&&index<=65535&&index==std::floor(index)&&std::isfinite(value)&&cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row,"read_available"))){
            char key[24];snprintf(key,sizeof(key),"menu:%.0f",index);cJSON_AddNumberToObject(out,key,value);
        }
    }
    cJSON_ArrayForEach(row,cJSON_GetObjectItemCaseSensitive(data,"targets")){
        const double value=number(row,"before");
        if(row->string&&std::isfinite(value)&&!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row,"cached"))){const auto key=std::string("target:")+row->string;cJSON_AddNumberToObject(out,key.c_str(),value);}
    }
    if(!cJSON_GetArraySize(out)){cJSON_Delete(out);return nullptr;}return out;
}
inline cJSON *difference(const cJSON *values,const cJSON *previous,bool baseline){
    auto *out=cJSON_CreateObject();const cJSON *row;
    cJSON_ArrayForEach(row,values)if(baseline||!cJSON_Compare(row,cJSON_GetObjectItemCaseSensitive(previous,row->string),true))cJSON_AddItemToObject(out,row->string,cJSON_Duplicate(row,true));
    if(!baseline)cJSON_ArrayForEach(row,previous)if(!cJSON_HasObjectItem(values,row->string))cJSON_AddNullToObject(out,row->string);
    return out;
}
}
