#include "controller_profile.hpp"
#include "model810.hpp"
#include <cassert>
#include <cstdio>
using namespace controller_profile;
void controller_profile_tests(){
 for(auto model:{"ecoMAX810P-L TOUCH","ecoMAX860P-LZ TOUCH","ecoMAX362P1","ecoMAX 920P","EM860P"})assert(family(0,model));
 assert(!family(1,"ecoMAX810P-L TOUCH")&&!family(0,"other")&&!family(0,"ecoMAX")&&!family(0,"ecoMAX123\n"));
 auto *sensor=cJSON_Parse(R"({"fresh":true,"age_ms":100,"modules":{"module_a":"10.40.186.R5","panel":"10.14.50"},"sensors":{"state":0,"pending_alerts":0},"outputs":{"alarm":false}})");
 assert(!control_guard(sensor,true,true,true,true)&&!control_guard(sensor,true,true,true,false));
 auto *mods=cJSON_GetObjectItemCaseSensitive(sensor,"modules");
 for(auto version:{"8.41.38.L0","10.40.186.R5","11.99.1"}){cJSON_ReplaceItemInObject(mods,"module_a",cJSON_CreateString(version));assert(!control_guard(sensor,true,true,true,true));}
 assert(control_guard(sensor,true,false,true,true)&&control_guard(sensor,true,true,false,true));
 cJSON_ReplaceItemInObject(mods,"module_a",cJSON_CreateString("unknown"));assert(!strcmp(control_guard(sensor,true,true,true,true),"Brak odczytu wersji modulu A po RS485"));
 cJSON_ReplaceItemInObject(mods,"module_a",cJSON_CreateString("10.40.186.R5"));
 cJSON_ReplaceItemInObject(sensor,"age_ms",cJSON_CreateNull());assert(!strcmp(control_guard(sensor,true,true,true,true),"Brak aktualnego odczytu stanu kotla po RS485"));
 cJSON_ReplaceItemInObject(sensor,"age_ms",cJSON_CreateNumber(100));
 cJSON_ReplaceItemInObject(cJSON_GetObjectItemCaseSensitive(sensor,"sensors"),"pending_alerts",cJSON_CreateNumber(1));assert(control_guard(sensor,true,true,true,true)&&!control_guard(sensor,true,true,true,false));
 cJSON_Delete(sensor);
 auto *rows=cJSON_CreateArray();
 for(unsigned i=0;i<model810::parameter_count;++i){auto *r=cJSON_CreateObject();auto &m=model810::metadata[i];cJSON_AddNumberToObject(r,"index",i);cJSON_AddNumberToObject(r,"unit",m.unit);cJSON_AddNumberToObject(r,"mult",m.mult);cJSON_AddNumberToObject(r,"offset",m.offset);cJSON_AddItemToArray(rows,r);}
 assert(metadata_valid(rows));auto *copy=cJSON_Duplicate(rows,true);assert(same_metadata(rows,copy));cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(copy,0),"offset"),1);assert(!same_metadata(rows,copy));cJSON_Delete(copy);
 for(int i=169;i<255;++i){auto *r=cJSON_Duplicate(cJSON_GetArrayItem(rows,0),true);cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(r,"index"),i);cJSON_AddItemToArray(rows,r);}
 assert(metadata_valid(rows));cJSON_AddItemToArray(rows,cJSON_Duplicate(cJSON_GetArrayItem(rows,0),true));assert(!metadata_valid(rows));cJSON_Delete(rows);
 auto *versions=cJSON_Parse(R"({"command":0,"minor":0,"major":1,"commands":[]})");auto *commands=cJSON_GetObjectItemCaseSensitive(versions,"commands");
 for(auto command:catalog_commands){auto *r=cJSON_CreateObject();cJSON_AddNumberToObject(r,"command",command);cJSON_AddNumberToObject(r,"version",100+command);cJSON_AddItemToArray(commands,r);}
 assert(versions_valid(versions));copy=cJSON_Duplicate(versions,true);assert(same_versions(versions,copy));
 cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(copy,"commands"),0),"version"),900);assert(!same_versions(versions,copy));cJSON_Delete(copy);
 cJSON_AddItemToArray(commands,cJSON_Duplicate(cJSON_GetArrayItem(commands,0),true));assert(!versions_valid(versions));cJSON_Delete(versions);
 puts("PASS controller profiles: common START/STOP across firmware versions, stale/missing identity/alarm guards, own metadata/versions, 169 and 255 rows");
}
