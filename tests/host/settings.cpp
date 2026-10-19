#include "settings_journal.hpp"
#include "settings_observation.hpp"
#include <cassert>
#include <string>

namespace {
uint32_t test_hash(const void *data,size_t size,uint32_t seed){
    auto hash=seed?seed:2166136261U;const auto *bytes=static_cast<const uint8_t *>(data);
    for(size_t i=0;i<size;i++)hash=(hash^bytes[i])*16777619U;return hash;
}
}
void settings_journal_tests(){
    constexpr uint32_t time=1700000000,hour=time/3600*3600;
    auto *data=cJSON_Parse(R"({"target_read_epoch":1700000000,"menu_read_epoch":1700000000,"password":"PRIVATE","menu":[{"index":12,"read_available":true,"metadata":{"value":0}},{"index":13,"read_available":false,"metadata":{"value":44}}],"targets":{"1281":{"before":45},"room":{"before":22,"cached":true}}})");
    bool fresh=false;auto *values=settings_observation::values(data,true,time,fresh);assert(fresh&&values&&cJSON_GetArraySize(values)==2);
    assert(cJSON_GetObjectItem(values,"menu:12")->valuedouble==0);assert(!cJSON_HasObjectItem(values,"menu:13")&&!cJSON_HasObjectItem(values,"target:room")&&!cJSON_HasObjectItem(values,"password"));
    auto *delta=settings_observation::difference(values,nullptr,true);assert(cJSON_GetArraySize(delta)==2);cJSON_Delete(delta);
    delta=settings_observation::difference(values,values,false);assert(cJSON_GetArraySize(delta)==0);cJSON_Delete(delta);
    auto *changed=cJSON_Duplicate(values,true);cJSON_ReplaceItemInObject(changed,"target:1281",cJSON_CreateNumber(46));cJSON_DeleteItemFromObject(changed,"menu:12");
    delta=settings_observation::difference(changed,values,false);assert(cJSON_GetArraySize(delta)==2&&cJSON_IsNull(cJSON_GetObjectItem(delta,"menu:12")));assert(cJSON_GetObjectItem(delta,"target:1281")->valuedouble==46);cJSON_Delete(delta);cJSON_Delete(changed);cJSON_Delete(values);
    assert(!settings_observation::values(data,true,time+21,fresh));assert(!settings_observation::values(data,true,time-3,fresh));
    values=settings_observation::values(data,false,time,fresh);assert(values&&!fresh&&cJSON_GetArraySize(values)==1);cJSON_Delete(values);
    cJSON_SetNumberValue(cJSON_GetObjectItem(data,"menu_read_epoch"),time-21);values=settings_observation::values(data,true,time,fresh);assert(values&&!fresh&&cJSON_GetArraySize(values)==1);cJSON_Delete(values);cJSON_Delete(data);
    const char *baseline="{\"kind\":\"baseline\",\"values\":{\"menu:12\":32,\"target:1281\":45}}";
    auto *file=tmpfile();assert(file);
    assert(settings_journal::append(file,baseline,time,test_hash));
    assert(settings_journal::append(file,"{\"kind\":\"changes\",\"values\":{\"menu:12\":33}}",time+1,test_hash));
    auto *events=cJSON_CreateArray();auto result=settings_journal::scan(file,hour,events,hour,time+10,test_hash);
    assert(result.valid&&!result.partial&&result.count==2&&cJSON_GetArraySize(events)==2);
    assert(cJSON_GetObjectItem(cJSON_GetArrayItem(events,0),"time")->valuedouble==time);
    assert(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetArrayItem(events,1),"values"),"menu:12")->valuedouble==33);
    cJSON_Delete(events);
    assert(fseek(file,0,SEEK_END)==0);assert(fwrite("partial",7,1,file)==1);fflush(file);
    result=settings_journal::scan(file,hour,nullptr,0,UINT32_MAX,test_hash);assert(result.valid&&result.partial&&result.count==2);
    assert(settings_journal::append(file,"{\"kind\":\"observed\",\"values\":{}}",time+2,test_hash));
    result=settings_journal::scan(file,hour,nullptr,0,UINT32_MAX,test_hash);assert(result.valid&&!result.partial&&result.count==3);
    assert(!settings_journal::append(file,"not JSON",time+3,test_hash));
    assert(!settings_journal::append(file,baseline,0,test_hash));
    assert(!settings_journal::append(file,baseline,hour+3600,test_hash));
    assert(fseek(file,sizeof(settings_journal::Header)+10,SEEK_SET)==0);int before=fgetc(file);assert(before!=EOF);
    assert(fseek(file,-1,SEEK_CUR)==0);fputc(before^1,file);fflush(file);
    result=settings_journal::scan(file,hour,nullptr,0,UINT32_MAX,test_hash);assert(!result.valid);
    assert(!settings_journal::append(file,baseline,time+3,test_hash));fclose(file);
    file=tmpfile();assert(file);std::string large="{\"padding\":\""+std::string(11000,'x')+"\"}";
    assert(settings_journal::append(file,large.c_str(),time,test_hash));assert(settings_journal::append(file,large.c_str(),time+1,test_hash));
    assert(!settings_journal::append(file,large.c_str(),time+2,test_hash));
    result=settings_journal::scan(file,hour,nullptr,0,UINT32_MAX,test_hash);assert(result.valid&&result.count==2&&result.complete<=settings_journal::hour_limit);
    fclose(file);puts("PASS settings journal: persisted records, deltas, short transitions, torn-tail recovery, corruption and size bounds");
}
