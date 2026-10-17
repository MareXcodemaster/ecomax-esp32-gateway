#pragma once
#include "cJSON.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace update_manifest {
constexpr const char *repository = "MareXcodemaster/ecomax-esp32-gateway";
struct Version { unsigned part[3]{}; bool prerelease=false; std::string suffix; };
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
    out.prerelease=true;out.suffix=text;
    if(out.suffix.size()>31)return false;
    size_t begin=0;
    for(size_t i=0;i<=out.suffix.size();++i){
        if(i<out.suffix.size()&&out.suffix[i]!='.'){
            char c=out.suffix[i];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
        }else{
            auto item=out.suffix.substr(begin,i-begin);if(item.empty())return false;
            if(item.size()>1&&item[0]=='0'&&item.find_first_not_of("0123456789")==std::string::npos)return false;
            begin=i+1;
        }
    }
    return true;
}
inline bool newer(const char *candidate,const char *current) {
    Version a,b;
    if (!version(candidate,a)||!version(current,b)) return false;
    for(unsigned i=0;i<3;++i)if(a.part[i]!=b.part[i])return a.part[i]>b.part[i];
    if(a.prerelease!=b.prerelease)return b.prerelease;
    if(!a.prerelease)return false;
    size_t left=0,right=0;
    while(left<a.suffix.size()&&right<b.suffix.size()){
        auto le=a.suffix.find('.',left),re=b.suffix.find('.',right);
        auto x=a.suffix.substr(left,le-left),y=b.suffix.substr(right,re-right);
        bool xn=x.find_first_not_of("0123456789")==std::string::npos,yn=y.find_first_not_of("0123456789")==std::string::npos;
        if(x!=y){if(xn!=yn)return !xn;if(xn&&x.size()!=y.size())return x.size()>y.size();return x>y;}
        if(le==std::string::npos||re==std::string::npos)return le!=std::string::npos;
        left=le+1;right=re+1;
    }
    return false;
}
inline bool download_url(const char *url) {
    if(!url||std::strlen(url)>2047)return false;
    const std::string value(url);
    return value.rfind("https://release-assets.githubusercontent.com/",0)==0||
        value.rfind("https://github.com/MareXcodemaster/ecomax-esp32-gateway/releases/download/",0)==0;
}
struct Candidate { char version[32]{},url[256]{},sha256[65]{}; size_t size=0; bool beta=false; };
inline bool parse(const cJSON *root,Candidate &out,bool include_beta=false) {
    out={};
    const auto *pre=cJSON_GetObjectItemCaseSensitive(root,"prerelease"),*draft=cJSON_GetObjectItemCaseSensitive(root,"draft");
    const auto *tag=cJSON_GetObjectItemCaseSensitive(root,"tag_name");
    Version parsed;
    if(!cJSON_IsBool(pre)||(!include_beta&&cJSON_IsTrue(pre))||!cJSON_IsBool(draft)||cJSON_IsTrue(draft)||
       !cJSON_IsString(tag)||tag->valuestring[0]!='v'||!version(tag->valuestring+1,parsed)||parsed.prerelease!=cJSON_IsTrue(pre)||
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
        std::memcpy(out.sha256,hash->valuestring+7,65);out.size=static_cast<size_t>(size->valuedouble);out.beta=parsed.prerelease;found=true;
    }
    return found;
}
inline bool select(const cJSON *root,Candidate &out,bool include_beta){
    out={};if(!cJSON_IsArray(root))return false;
    bool found=false;const cJSON *item=nullptr;
    cJSON_ArrayForEach(item,root){Candidate next;if(parse(item,next,include_beta)&&(!found||newer(next.version,out.version))){out=next;found=true;}}
    return found;
}
struct ReleasePages {
    static constexpr unsigned page_size=5,max_pages=10;
    Candidate best{};
    unsigned page=1;
    bool found=false,complete=false;
    bool can_request() const { return !complete&&page<=max_pages; }
    bool append(const cJSON *root) {
        if(!can_request()||!cJSON_IsArray(root)||cJSON_GetArraySize(root)>static_cast<int>(page_size))return false;
        Candidate next;
        if(select(root,next,true)&&(!found||newer(next.version,best.version))){best=next;found=true;}
        complete=cJSON_GetArraySize(root)<static_cast<int>(page_size);
        ++page;
        return true;
    }
};
}
