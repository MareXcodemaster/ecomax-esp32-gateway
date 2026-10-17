#include "protocol.hpp"
#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <cmath>

namespace {
using Bytes=std::vector<uint8_t>;
Bytes frame(unsigned type,const Bytes &payload,unsigned source=69,unsigned destination=0) {
    unsigned size=payload.size()+10;
    Bytes result={0x68,static_cast<uint8_t>(size),static_cast<uint8_t>(size>>8),static_cast<uint8_t>(destination),static_cast<uint8_t>(source),48,5,static_cast<uint8_t>(type)};
    result.insert(result.end(),payload.begin(),payload.end()); uint8_t sum=0;
    for(auto byte:result)sum^=byte;
    result.push_back(sum);result.push_back(0x16);return result;
}
cJSON *describe(unsigned type,const Bytes &payload,const char *state,unsigned source=69,unsigned destination=0) {
    auto bytes=frame(type,payload,source,destination);auto *result=protocol::describe(bytes.data(),bytes.size());
    assert(result&&!strcmp(cJSON_GetObjectItemCaseSensitive(result,"state")->valuestring,state));return result;
}
void state(unsigned type,const Bytes &payload,const char *wanted,unsigned source=69,unsigned destination=0) { cJSON_Delete(describe(type,payload,wanted,source,destination)); }
cJSON *get(cJSON *object,const char *key) { return cJSON_GetObjectItemCaseSensitive(object,key); }
void floats(Bytes &bytes,float value) { uint32_t bits;memcpy(&bits,&value,4);for(unsigned shift=0;shift<32;shift+=8)bytes.push_back(bits>>shift); }
}

void diagnostics_tests() {
    Bytes product={0,42,0,3,1,2,3,2,0,3,0,11};
    const std::string model="ecoMAX DEMO";product.back()=model.size();product.insert(product.end(),model.begin(),model.end());
    auto *root=describe(185,product,"decoded"),*data=get(root,"data");
    assert(!strcmp(get(data,"model")->valuestring,model.c_str())&&get(data,"product_id")->valueint==42);
    assert(strlen(get(data,"uid")->valuestring)>0);cJSON_Delete(root);
    for(size_t size=0;size<product.size();++size)state(185,Bytes(product.begin(),product.begin()+size),"payload_mismatch");
    auto bad=product;bad.push_back(0);state(185,bad,"payload_mismatch");bad=product;bad[3]=255;state(185,bad,"payload_mismatch");

    Bytes modules;
    for(unsigned version=0;version<=5;++version) {
        if(version)modules.insert(modules.end(),{255,255,static_cast<uint8_t>(version)});
        modules.insert(modules.end(),{42,0,1,2,3});if(version==3)modules.push_back(4);
        if(version==5)modules.insert(modules.end(),{9,0,12,0,34,0});
        else { modules.push_back(0x12);if(version>=2)modules.push_back(0);modules.insert(modules.end(),{34,9,1,0}); }
        if(version!=5){const std::string date="Jan  1 2020 00:00:00";modules.insert(modules.end(),date.begin(),date.end());}
        modules.push_back(version==4?0x59:0x45);
    }
    root=describe(192,modules,"decoded");auto *list=get(get(root,"data"),"modules");assert(cJSON_GetArraySize(list)==6);
    for(unsigned i=0;i<6;++i){auto *entry=cJSON_GetArrayItem(list,i);assert(get(entry,"structure_version")->valueint==int(i));assert(!strcmp(get(entry,"software")->valuestring,"9.12.34"));}
    cJSON_Delete(root);state(192,{},"payload_mismatch");state(192,{255,255,6},"payload_mismatch");
    bad=modules;bad.pop_back();state(192,bad,"payload_mismatch");
    for(size_t size=0;size<modules.size();++size){auto b=frame(192,Bytes(modules.begin(),modules.begin()+size));cJSON_Delete(protocol::describe(b.data(),b.size()));}

    Bytes time={'T','I','M','E',12,34,56,0xe4,7,2,29,6};
    root=describe(137,time,"partial",0x50);assert(!strcmp(get(get(root,"data"),"device_time")->valuestring,"2020-02-29 12:34:56"));cJSON_Delete(root);
    auto room=time;room.insert(room.end(),{0,1,2,3});floats(room,20.5f);floats(room,19.75f);floats(room,19.75f);floats(room,0);
    const std::string thermostat="ecoSTER DEMO";room.insert(room.end(),thermostat.begin(),thermostat.end());room.insert(room.end(),{0,1,2});
    root=describe(137,room,"partial",0x51);data=get(root,"data");
    assert(get(data,"target_temperature")->valuedouble==20.5&&get(data,"room_temperature")->valuedouble==19.75);
    assert(cJSON_GetArraySize(get(data,"unknown_fields"))==3);cJSON_Delete(root);
    bad=time;bad[7]=0xe3;state(137,bad,"payload_mismatch",0x50);bad=time;bad[9]=13;state(137,bad,"payload_mismatch",0x50);
    bad=room;bad[16]=0;bad[17]=0;bad[18]=0xc0;bad[19]=0x7f;state(137,bad,"payload_mismatch",0x51);
    for(size_t size=0;size<room.size()-3;++size)state(137,Bytes(room.begin(),room.begin()+size),"payload_mismatch",0x51);
    bad=time;bad[0]='X';state(137,bad,"payload_mismatch",0x50);

    Bytes query={1,2,3,4,5};state(106,query,"partial",0x50,0x59);
    auto reply=query;reply.insert(reply.end(),9,0);state(234,reply,"partial",0x59,0x50);
    state(106,query,"payload_mismatch",69,0x59);state(234,query,"payload_mismatch",0x59,0x50);
    Bytes info={1,9,12,34,static_cast<uint8_t>(model.size())};info.insert(info.end(),model.begin(),model.end());info.push_back(0);
    root=describe(107,info,"partial",69,0x59);assert(!strcmp(get(get(root,"data"),"software_observed")->valuestring,"9.12.34"));cJSON_Delete(root);
    state(107,info,"payload_mismatch",69,0x50);state(235,{1,2,3},"partial",0x59);state(235,{},"payload_mismatch",0x59);
    state(10,{},"partial");state(10,{1},"payload_mismatch");state(240,{1},"not_documented");
    auto bytes=frame(137,room,0x51);protocol::receive(bytes.data(),bytes.size());
    auto *stats=cJSON_CreateObject();protocol::add_stats(stats,137);
    assert(get(stats,"partial_frames")->valueint==1&&!cJSON_IsTrue(get(stats,"decoded")));
    assert(get(stats,"label")&&get(stats,"description"));cJSON_Delete(stats);
    bytes[bytes.size()-2]^=1;root=protocol::describe(bytes.data(),bytes.size());assert(!strcmp(get(root,"state")->valuestring,"payload_mismatch"));cJSON_Delete(root);
}
