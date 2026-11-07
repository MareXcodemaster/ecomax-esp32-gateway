#pragma once
#include "cJSON.h"
#include <cstdint>
#include <cstring>
#include <cstdio>

// A complete live snapshot plus a versioned presentation catalogue. Never use
// this cache to authorize writes: the existing server-side preflight still does.
namespace panel_sync {
inline bool listed(const char *key,const char *const *keys,size_t size){
    if(!key)return false;
    for(size_t i=0;i<size;++i)if(!strcmp(key,keys[i]))return true;
    return false;
}
inline bool catalogue_key(const char *key){
    static const char *const keys[]={"names","units","enums","structure","categories","category_names","category_descriptions","alarm_names","tiles","write_allowlist"};
    return listed(key,keys,sizeof(keys)/sizeof(*keys));
}
inline bool menu_live(const char *key){
    static const char *const keys[]={"enum_text","read_available","read_status","write_enabled","cached","value_source"};
    return listed(key,keys,sizeof(keys)/sizeof(*keys));
}
inline bool target_static(const char *key){
    static const char *const keys[]={"id","name","unit","min","max","step","options","schema","endpoint","wire_key","value_kind","generic","risk","menu_index","advanced","wire_unit","wire_mult","wire_offset"};
    return listed(key,keys,sizeof(keys)/sizeof(*keys));
}
inline cJSON *get(const cJSON *v,const char *key){return cJSON_GetObjectItemCaseSensitive(v,key);}
struct Builder {
    bool ok=true;
    cJSON *duplicate(const cJSON *v){auto *p=v?cJSON_Duplicate(v,true):cJSON_CreateNull();if(!p)ok=false;return p;}
    void add(cJSON *out,const char *key,cJSON *v){if(!v||!cJSON_AddItemToObject(out,key,v)){ok=false;cJSON_Delete(v);}}
    void copy(cJSON *out,const char *key,const cJSON *v){add(out,key,duplicate(v));}
    cJSON *row(const cJSON *source,bool catalog,bool target){
        auto *out=cJSON_CreateObject();if(!out){ok=false;return nullptr;}
        const cJSON *p=nullptr;
        cJSON_ArrayForEach(p,source){
            if(!target&&!strcmp(p->string,"metadata")){
                if(!cJSON_IsObject(p)){if(!catalog)copy(out,"metadata",p);continue;}
                auto *meta=cJSON_CreateObject();const cJSON *m=nullptr;
                cJSON_ArrayForEach(m,p)if((strcmp(m->string,"value")!=0)==catalog)copy(meta,m->string,m);
                add(out,"metadata",meta);
            }else if((target?target_static(p->string):!menu_live(p->string))==catalog||(!target&&!strcmp(p->string,"index")))copy(out,p->string,p);
        }
        return out;
    }
};
// Hash only catalogue members, directly from cJSON: no temporary strings or
// duplicate full catalogue on normal polls. Object order is deterministic in
// our builders. A hash is a cache key, not a security/identity assertion.
struct Hash {
    uint64_t value=14695981039346656037ULL;
    void bytes(const void *data,size_t size){auto *p=static_cast<const uint8_t*>(data);while(size--){value^=*p++;value*=1099511628211ULL;}}
    void text(const char *s){if(s)bytes(s,strlen(s));const uint8_t end=0;bytes(&end,1);}
    void node(const cJSON *p){
        if(!p){text("missing");return;}
        auto type=p->type&255;bytes(&type,sizeof(type));text(p->string);
        if(cJSON_IsNumber(p))bytes(&p->valuedouble,sizeof(p->valuedouble));
        else if(cJSON_IsString(p))text(p->valuestring);
        const cJSON *v=nullptr;cJSON_ArrayForEach(v,p)node(v);
        const uint8_t end=255;bytes(&end,1);
    }
    void row(const cJSON *r,bool target){
        text(r->string);const cJSON *p=nullptr;
        cJSON_ArrayForEach(p,r){
            if(!target&&!strcmp(p->string,"metadata")){text("metadata");const cJSON *m=nullptr;cJSON_ArrayForEach(m,p)if(strcmp(m->string,"value"))node(m);}
            else if(target?target_static(p->string):!menu_live(p->string))node(p);
        }
        text("row-end");
    }
};
inline cJSON *snapshot(const cJSON *source,const char *known,const char *boot){
    Hash hash;hash.text(boot);
    // Include identity even where two models happen to expose identical names.
    hash.node(get(source,"system"));hash.node(get(source,"write_transport"));
    const cJSON *p=nullptr;
    cJSON_ArrayForEach(p,source){
        if(catalogue_key(p->string))hash.node(p);
        else if(!strcmp(p->string,"menu")||!strcmp(p->string,"targets")){
            hash.text(p->string);const cJSON *r=nullptr;
            cJSON_ArrayForEach(r,p)hash.row(r,!strcmp(p->string,"targets"));
        }
    }
    char token[17];snprintf(token,sizeof(token),"%016llx",static_cast<unsigned long long>(hash.value));
    const bool include=!known||strcmp(token,known);
    Builder b;auto *out=cJSON_CreateObject(),*catalog=include?cJSON_CreateObject():nullptr;
    if(!out||(include&&!catalog)){cJSON_Delete(out);cJSON_Delete(catalog);return nullptr;}
    cJSON_ArrayForEach(p,source){
        if(catalogue_key(p->string)){if(include)b.copy(catalog,p->string,p);continue;}
        bool menu=!strcmp(p->string,"menu"),target=!strcmp(p->string,"targets");
        if(menu||target){
            auto *live=menu?cJSON_CreateArray():cJSON_CreateObject();
            auto *defs=include?(menu?cJSON_CreateArray():cJSON_CreateObject()):nullptr;
            const cJSON *r=nullptr;cJSON_ArrayForEach(r,p){
                auto *v=b.row(r,false,target);
                if(menu){if(!v||!cJSON_AddItemToArray(live,v)){cJSON_Delete(v);b.ok=false;}}else b.add(live,r->string,v);
                if(include){v=b.row(r,true,target);if(menu){if(!v||!cJSON_AddItemToArray(defs,v)){cJSON_Delete(v);b.ok=false;}}else b.add(defs,r->string,v);}
            }
            b.add(out,p->string,live);if(include)b.add(catalog,p->string,defs);
        }else b.copy(out,p->string,p);
    }
    auto *sync=cJSON_CreateObject();if(!sync)b.ok=false;
    if(!cJSON_AddNumberToObject(sync,"schema",1)||!cJSON_AddStringToObject(sync,"catalog",token)||!cJSON_AddBoolToObject(sync,"included",include))b.ok=false;
    b.add(out,"sync",sync);if(include)b.add(out,"catalog",catalog);
    if(!b.ok){cJSON_Delete(out);return nullptr;}return out;
}
}
