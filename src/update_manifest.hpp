#pragma once
#include "cJSON.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace update_manifest {
constexpr const char *repository = "MareXcodemaster/ecomax-esp32-gateway";
struct Version { unsigned part[3]{}; bool prerelease=false; };
inline bool version(const char *text, Version &out, bool stable=false) {
    if (!text) return false;
    out={};
    for (unsigned i=0;i<3;++i) {
        const char *first=text;
        if (*text<'0'||*text>'9') return false;
        while (*text>='0'&&*text<='9') {
            out.part[i]=out.part[i]*10+(*text++-'0');
            if (out.part[i]>65535) return false;
        }
        if (text-first>1&&*first=='0') return false;
        if (i<2&&*text++!='.') return false;
    }
    if (!*text) return true;
    if (stable||*text++!='-'||!*text) return false;
    out.prerelease=true;
    for (;*text;++text) if (!((*text>='a'&&*text<='z')||(*text>='0'&&*text<='9')||*text=='.'||*text=='-')) return false;
    return true;
}
inline bool newer(const char *candidate,const char *current) {
    Version a,b;
    if (!version(candidate,a,true)||!version(current,b)) return false;
    for(unsigned i=0;i<3;++i)if(a.part[i]!=b.part[i])return a.part[i]>b.part[i];
    return b.prerelease;
}
inline bool download_url(const char *url) {
    if(!url||std::strlen(url)>2047)return false;
    const std::string value(url);
    return value.rfind("https://release-assets.githubusercontent.com/",0)==0||
        value.rfind("https://github.com/MareXcodemaster/ecomax-esp32-gateway/releases/download/",0)==0;
}
struct Candidate { char version[32]{},url[256]{},sha256[65]{}; size_t size=0; };
inline bool parse(const cJSON *root,Candidate &out) {
    out={};
    const auto *pre=cJSON_GetObjectItemCaseSensitive(root,"prerelease"),*draft=cJSON_GetObjectItemCaseSensitive(root,"draft");
    const auto *tag=cJSON_GetObjectItemCaseSensitive(root,"tag_name");
    Version parsed;
    if(!cJSON_IsBool(pre)||cJSON_IsTrue(pre)||!cJSON_IsBool(draft)||cJSON_IsTrue(draft)||
       !cJSON_IsString(tag)||tag->valuestring[0]!='v'||!version(tag->valuestring+1,parsed,true)||
       std::strlen(tag->valuestring+1)>=sizeof(out.version))return false;
    const auto *assets=cJSON_GetObjectItemCaseSensitive(root,"assets");
    if(!cJSON_IsArray(assets))return false;
    const cJSON *asset=nullptr; bool found=false;
    cJSON_ArrayForEach(asset,assets) {
        const auto *name=cJSON_GetObjectItemCaseSensitive(asset,"name");
        if(!cJSON_IsString(name)||std::strcmp(name->valuestring,"firmware-ota.bin"))continue;
        if(found)return false;
        const auto *url=cJSON_GetObjectItemCaseSensitive(asset,"browser_download_url");
        const auto *size=cJSON_GetObjectItemCaseSensitive(asset,"size"),*hash=cJSON_GetObjectItemCaseSensitive(asset,"digest");
        std::string expected="https://github.com/";expected+=repository;expected+="/releases/download/";expected+=tag->valuestring;expected+="/firmware-ota.bin";
        if(!cJSON_IsString(url)||expected!=url->valuestring||expected.size()>=sizeof(out.url)||
           !cJSON_IsNumber(size)||!std::isfinite(size->valuedouble)||size->valuedouble<=1024||size->valuedouble>=0x300000||
           std::floor(size->valuedouble)!=size->valuedouble||!cJSON_IsString(hash)||
           std::strlen(hash->valuestring)!=71||std::strncmp(hash->valuestring,"sha256:",7))return false;
        for(unsigned i=7;i<71;++i)if(!((hash->valuestring[i]>='0'&&hash->valuestring[i]<='9')||(hash->valuestring[i]>='a'&&hash->valuestring[i]<='f')))return false;
        std::snprintf(out.version,sizeof(out.version),"%s",tag->valuestring+1);
        std::snprintf(out.url,sizeof(out.url),"%s",url->valuestring);
        std::memcpy(out.sha256,hash->valuestring+7,65);out.size=static_cast<size_t>(size->valuedouble);found=true;
    }
    return found;
}
}
