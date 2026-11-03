#pragma once
#include "cJSON.h"
#include "protocol.hpp"
#include <cstdint>
#include <cstring>
#include <string>

// A fixed list of READS, independent of write profiles. No caller-supplied opcode.
namespace compatibility_report {
struct Query { const char *name; uint8_t type,response,size,payload[3]; };
constexpr Query queries[]={
 {"product",57,185,0,{}},{"modules",64,192,0,{}},
 {"field_schema",85,213,0,{}},{"current_parameters",84,212,0,{}},
 {"menu_versions",86,214,1,{0}},{"menu_metadata",86,214,1,{3}},
 {"menu_values",86,214,3,{4,255,0}},{"menu_structure",86,214,1,{9}},
 {"thermostat_values",92,220,2,{255,0}},{"thermostat_metadata",94,222,2,{255,0}},
 {"thermostat_structure",95,223,2,{255,0}},
 {"mixer_values",50,178,2,{16,0}},{"mixer_metadata",81,209,2,{16,0}},
 {"mixer_structure",83,211,2,{16,0}}
};
inline const cJSON *get(const cJSON *o,const char *k){return cJSON_GetObjectItemCaseSensitive(o,k);}
inline void copy(cJSON *out,const cJSON *in,const char *key){auto *v=get(in,key);if(v)cJSON_AddItemToObject(out,key,cJSON_Duplicate(v,true));}
inline cJSON *numeric(const cJSON *in,unsigned depth=0){
 if(depth>16)return cJSON_CreateNull();
 if(cJSON_IsNumber(in)||cJSON_IsBool(in)||cJSON_IsNull(in))return cJSON_Duplicate(in,true);
 auto *out=cJSON_IsArray(in)?cJSON_CreateArray():cJSON_CreateObject();unsigned count=0;
 const cJSON *v; cJSON_ArrayForEach(v,in){if(++count>1024)break;
  if(cJSON_IsString(v))continue;
  auto *child=numeric(v,depth+1);if(cJSON_IsArray(in))cJSON_AddItemToArray(out,child);
  else if(v->string)cJSON_AddItemToObject(out,v->string,child);else cJSON_Delete(child);
 }return out;
}
inline bool raw_allowed(unsigned type,const uint8_t *p,size_t n){
 if(type==214)return n&&(p[0]==0||p[0]==3||p[0]==4||p[0]==9);
 return type==178||type==209||type==211||type==212||type==213||type==220||type==222||type==223;
}
inline cJSON *frame(const uint8_t *bytes,size_t size){
 if(!bytes||size<10||size>1000||bytes[0]!=0x68||bytes[size-1]!=0x16||size!=(bytes[1]|(size_t(bytes[2])<<8)))return nullptr;
 uint8_t checksum=0;for(size_t i=0;i<size-2;++i)checksum^=bytes[i];if(checksum!=bytes[size-2])return nullptr;
 unsigned type=bytes[7];if(bytes[4]!=69&&bytes[4]!=80&&bytes[4]!=81)return nullptr;
 if(type!=185&&type!=192&&type!=53&&!raw_allowed(type,bytes+8,size-10))return nullptr;
 auto *out=cJSON_CreateObject(),*decoded=protocol::describe(bytes,size);
 copy(out,decoded,"state");
 cJSON_AddNumberToObject(out,"type",type);cJSON_AddNumberToObject(out,"source_address",bytes[4]);
 cJSON_AddNumberToObject(out,"destination_address",bytes[3]);cJSON_AddNumberToObject(out,"length_bytes",size);
 const auto *data=get(decoded,"data");auto *safe=cJSON_CreateObject();
 if(type==185){for(auto key:{"product_type","product_id","logo_id","image_id","model"})copy(safe,data,key);cJSON_AddBoolToObject(out,"identity_removed",true);}
 else if(type==192){auto *list=cJSON_AddArrayToObject(safe,"modules");const cJSON *m;cJSON_ArrayForEach(m,get(data,"modules")){auto *v=cJSON_CreateObject();for(auto key:{"address","device","structure_version","hardware_bytes","software","bootloader_raw","build_date"})copy(v,m,key);cJSON_AddItemToArray(list,v);}}
 else {cJSON_Delete(safe);safe=numeric(data);}
 cJSON_AddItemToObject(out,"decoded",safe);cJSON_Delete(decoded);
 // Product and module raw bytes are deliberately excluded: they can carry identity.
 if(raw_allowed(type,bytes+8,size-10)){
  const char digits[]="0123456789abcdef";std::string hex;hex.reserve(size*2);
  for(size_t i=0;i<size;++i){hex+=digits[bytes[i]>>4];hex+=digits[bytes[i]&15];}
  cJSON_AddStringToObject(out,"hex",hex.c_str());
 }
 return out;
}
}
