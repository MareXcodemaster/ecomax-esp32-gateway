#pragma once
#include "cJSON.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace settings_journal {
constexpr size_t max_payload=12000,hour_limit=32768;
using Hash=uint32_t(*)(const void *,size_t,uint32_t);
struct Header { uint32_t magic,time,length,checksum; };
static_assert(sizeof(Header)==16,"Settings journal ABI changed");
struct Scan { bool valid=true,partial=false;size_t complete=0,count=0; };
inline uint32_t checksum(const Header &header,const void *payload,Hash hash) {
    return hash(payload,header.length,hash(&header,offsetof(Header,checksum),0));
}
inline Scan scan(FILE *file,uint32_t hour,cJSON *entries,uint32_t from,uint32_t to,Hash hash) {
    Scan result;if(fseek(file,0,SEEK_SET)){result.valid=false;return result;}
    for(;;){
        Header header{};const auto position=ftell(file);auto got=fread(&header,1,sizeof(header),file);
        if(!got){if(ferror(file))result.valid=false;break;}
        if(got!=sizeof(header)){result.partial=true;break;}
        if(header.magic!=0x31545345||!header.length||header.length>max_payload||header.time<1577836800||header.time>4102444800U||header.time/3600*3600!=hour||position<0||static_cast<size_t>(position)+sizeof(header)+header.length>hour_limit){result.valid=false;break;}
        std::vector<char> payload(header.length+1,0);
        if(fread(payload.data(),1,header.length,file)!=header.length){result.partial=true;if(ferror(file))result.valid=false;break;}
        if(checksum(header,payload.data(),hash)!=header.checksum){result.valid=false;break;}
        auto *value=cJSON_ParseWithLength(payload.data(),payload.size());
        if(!cJSON_IsObject(value)){cJSON_Delete(value);result.valid=false;break;}
        ++result.count;result.complete=static_cast<size_t>(position)+sizeof(header)+header.length;
        if(entries&&header.time>=from&&header.time<=to){
            cJSON_DeleteItemFromObjectCaseSensitive(value,"time");cJSON_AddNumberToObject(value,"time",header.time);
            char id[40];snprintf(id,sizeof(id),"%lu:%lu",static_cast<unsigned long>(hour),static_cast<unsigned long>(position));cJSON_AddStringToObject(value,"event_id",id);
            cJSON_AddItemToArray(entries,value);
        }else cJSON_Delete(value);
    }
    return result;
}
inline bool append(FILE *file,const char *payload,uint32_t time,Hash hash) {
    const auto length=strlen(payload);if(!length||length>max_payload||time<1577836800||time>4102444800U)return false;
    auto *value=cJSON_ParseWithLength(payload,length+1);const bool object=cJSON_IsObject(value);cJSON_Delete(value);if(!object)return false;
    const auto previous=scan(file,time/3600*3600,nullptr,0,UINT32_MAX,hash);
    if(!previous.valid||previous.complete+sizeof(Header)+length>hour_limit)return false;
    if(previous.partial){
#ifdef _WIN32
        if(_chsize_s(_fileno(file),previous.complete))return false;
#else
        if(ftruncate(fileno(file),previous.complete))return false;
#endif
        clearerr(file);
    }
    if(fseek(file,previous.complete,SEEK_SET))return false;
    Header header{0x31545345,time,static_cast<uint32_t>(length),0};header.checksum=checksum(header,payload,hash);
    if(fwrite(&header,sizeof(header),1,file)!=1||fwrite(payload,length,1,file)!=1||fflush(file))return false;
#ifdef _WIN32
    return _commit(_fileno(file))==0;
#else
    return fsync(fileno(file))==0;
#endif
}
}
