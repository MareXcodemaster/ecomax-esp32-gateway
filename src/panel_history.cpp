#include "panel_history.hpp"
#include "settings_observation.hpp"
#include "history_store.hpp"
#include "state.hpp"
#include "esp_heap_caps.h"
#include "esp_rom_crc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <string>

namespace panel_history {
namespace {
constexpr unsigned capacity=720, interval_ms=10000;
struct Field { const char *key, *input; bool binary=false; double scale=1; };
const Field fields[]={
    {"co","tempCO"},{"coSet","tempCOSet"},{"cwu","tempCWU"},{"cwuSet","tempCWUSet"},
    {"outside","tempExternalSensor"},{"feederTemp","tempFeeder"},{"exhaust","tempFlueGas"},
    {"room","ecoSterTemp1"},{"roomSet","ecoSterSetTemp1"},{"mixer","mixerTemp1"},{"mixerSet","mixerSetTemp1"},
    {"power","boilerPower"},{"fan","fanPower"},{"fuel","fuelLevel"},{"flow","fuelStream"},
    {"kw","boilerPowerKW"},{"oxygen","lambdaLevel",false,0.1},
    {"pumpCO","pumpCOWorks",true},{"pumpCWU","pumpCWUWorks",true},{"feeder","feederWorks",true},
    {"lighter","lighterWorks",true},{"fanOn","fanWorks",true},{"mixerPump","mixerPumpWorks1",true},
    {"circulation","pumpCirculationWorks",true},{"alarm","alarmOutputWorks",true},
    {"room2","ecoSterTemp2"},{"roomSet2","ecoSterSetTemp2"},{"room3","ecoSterTemp3"},{"roomSet3","ecoSterSetTemp3"},
    {"mixer2","mixerTemp2"},{"mixerSet2","mixerSetTemp2"},{"mixerPump2","mixerPumpWorks2",true},
    {"mixer3","mixerTemp3"},{"mixerSet3","mixerSetTemp3"},{"mixerPump3","mixerPumpWorks3",true},
    {"mixer4","mixerTemp4"},{"mixerSet4","mixerSetTemp4"},{"mixerPump4","mixerPumpWorks4",true}
};
constexpr unsigned field_count=sizeof(fields)/sizeof(fields[0]);
static_assert(field_count==history_data::fields,"Archive field order/schema must be migrated together");
using history_data::Sample;using history_data::Point;
Sample *samples=nullptr;
SemaphoreHandle_t mutex=nullptr;
unsigned head=0,count=0;
uint64_t last_ms=0;
uint64_t archive_ms=0;
float previous_mode=NAN;
cJSON *settings_previous=nullptr;
uint32_t settings_hour=0;
uint64_t settings_ms=0;
std::string settings_context;
bool previous_native=false;
bool self_test();
float read(const cJSON *curr,const Field &field) {
    auto *value=cJSON_GetObjectItemCaseSensitive(curr,field.input);
    if(field.binary)return cJSON_IsBool(value)?(cJSON_IsTrue(value)?1:0):NAN;
    return cJSON_IsNumber(value)&&std::isfinite(value->valuedouble)?value->valuedouble*field.scale:NAN;
}
bool same(float a,float b) { return (std::isnan(a)&&std::isnan(b))||a==b; }
void add(cJSON *row,double value) {
    cJSON_AddItemToArray(row,std::isfinite(value)?cJSON_CreateNumber(std::round(value*100)/100):cJSON_CreateNull());
}
}
void start() {
    mutex=xSemaphoreCreateMutex();
    samples=static_cast<Sample *>(heap_caps_calloc(capacity,sizeof(Sample),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    state::log("HISTORY: bounded PSRAM buffer %u bytes, %u samples; %s",static_cast<unsigned>(capacity*sizeof(Sample)),capacity,mutex&&samples?"ready":"unavailable");
    if(mutex&&samples&&!self_test()){heap_caps_free(samples);samples=nullptr;}
    uint32_t signature=0;for(const auto &field:fields)signature=esp_rom_crc32_le(signature,reinterpret_cast<const uint8_t *>(field.key),strlen(field.key)+1);
    history_store::start(signature);
}
void clear() {
    if(!mutex)return;
    xSemaphoreTake(mutex,portMAX_DELAY); head=count=0;last_ms=archive_ms=0;previous_mode=NAN;xSemaphoreGive(mutex);
}
void append(const cJSON *curr,bool native,double epoch,uint64_t monotonic_ms) {
    if(!samples||!mutex||!cJSON_IsObject(curr)||!std::isfinite(epoch)||epoch<1577836800||epoch>4102444800)return;
    Sample sample{};sample.time=static_cast<uint32_t>(epoch);sample.native=native;
    auto *mode=cJSON_GetObjectItemCaseSensitive(curr,"mode");
    sample.mode=cJSON_IsNumber(mode)&&std::isfinite(mode->valuedouble)?mode->valuedouble:NAN;
    for(unsigned i=0;i<field_count;++i)sample.values[i]=read(curr,fields[i]);
    xSemaphoreTake(mutex,portMAX_DELAY);
    // Clock corrections start a new series; never draw a backwards time axis.
    if(count&&(sample.time<samples[(head+count-1)%capacity].time||monotonic_ms<last_ms))head=count=0;
    bool transition=count&&(!same(previous_mode,sample.mode)||previous_native!=native);
    if(!count||monotonic_ms-last_ms>=interval_ms||transition) {
        if(count==capacity){head=(head+1)%capacity;--count;}
        samples[(head+count++)%capacity]=sample;last_ms=monotonic_ms;previous_mode=sample.mode;previous_native=native;
    }
    bool save=!archive_ms||monotonic_ms<archive_ms||monotonic_ms-archive_ms>=60000;
    if(save)archive_ms=monotonic_ms;
    xSemaphoreGive(mutex);
    if(save)history_store::enqueue(sample);
}
void append_settings(const cJSON *data,bool native,double epoch,uint64_t monotonic_ms) {
    using settings_observation::number;using settings_observation::string;
    bool menu_fresh=false;auto *values=settings_observation::values(data,native,epoch,menu_fresh);if(!values)return;
    const auto *system=cJSON_GetObjectItemCaseSensitive(data,"system");
    const double target_epoch=number(data,"target_read_epoch"),menu_epoch=number(data,"menu_read_epoch");
    std::string context=(native?"rs485|":"econet|")+std::string(string(system,"controllerID"))+"|"+string(system,"moduleASoftVer")+"|"+string(system,"modulePanelSoftVer");
    const uint32_t hour=static_cast<uint32_t>(epoch)/3600*3600;
    const bool baseline=!settings_previous||settings_hour!=hour||settings_context!=context||monotonic_ms<settings_ms;
    auto *changes=settings_observation::difference(values,settings_previous,baseline);
    const bool changed=cJSON_GetArraySize(changes)>0;
    if(!changed&&monotonic_ms-settings_ms<600000){cJSON_Delete(changes);cJSON_Delete(values);return;}
    auto *event=cJSON_CreateObject();cJSON_AddNumberToObject(event,"schema",1);cJSON_AddStringToObject(event,"kind",baseline?"baseline":changed?"changes":"observed");
    cJSON_AddStringToObject(event,"transport",native?"rs485":"econet");cJSON_AddStringToObject(event,"model",string(system,"controllerID"));
    cJSON_AddStringToObject(event,"module_a",string(system,"moduleASoftVer"));cJSON_AddStringToObject(event,"panel",string(system,"modulePanelSoftVer"));
    cJSON_AddNumberToObject(event,"targets_observed_epoch",target_epoch);if(menu_fresh)cJSON_AddNumberToObject(event,"menu_observed_epoch",menu_epoch);
    cJSON_AddBoolToObject(event,"menu_included",menu_fresh);cJSON_AddItemToObject(event,"values",changes);
    char *json=cJSON_PrintUnformatted(event);cJSON_Delete(event);
    if(history_store::enqueue_settings(json,static_cast<uint32_t>(epoch))){cJSON_Delete(settings_previous);settings_previous=values;settings_hour=hour;settings_ms=monotonic_ms;settings_context=context;}
    else cJSON_Delete(values);
}
cJSON *snapshot(uint32_t from,uint32_t to,bool raw) {
    if(!samples||!mutex)return nullptr;
    if(!to)to=static_cast<uint32_t>(time(nullptr));
    if(!from)from=to>7200?to-7200:0;
    if(to<from||to-from>history_data::retention_seconds||(raw&&to-from>3600))return nullptr;
    auto *copy=static_cast<Point *>(heap_caps_malloc(capacity*2*sizeof(Point),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!copy)return nullptr;
    size_t size=history_store::read(from,to,copy,capacity);bool detail=false;
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(to-from<=7200)for(unsigned i=0;i<count;++i){auto sample=samples[(head+i)%capacity];if(sample.time>=from&&sample.time<=to){copy[size++]={sample,0};detail=true;}}
    xSemaphoreGive(mutex);
    std::stable_sort(copy,copy+size,[](const Point &a,const Point &b){return a.sample.time<b.sample.time;});
    unsigned unique=0;for(size_t i=0;i<size;++i){if(unique&&copy[unique-1].sample.time==copy[i].sample.time){copy[i].gap_before|=copy[unique-1].gap_before;copy[unique-1]=copy[i];}else copy[unique++]=copy[i];}
    const auto actual_from=unique?copy[0].sample.time:from,actual_to=unique?copy[unique-1].sample.time:to;
    auto resolution=std::max<uint32_t>(detail?10:60,history_data::step(actual_from,actual_to));
    if(raw){size=unique;for(unsigned i=1;i<size;++i)copy[i].gap_before|=copy[i].sample.time-copy[i-1].sample.time>90||copy[i].sample.native!=copy[i-1].sample.native;}
    else size=history_data::reduce(copy,unique,actual_from,actual_to,resolution,std::max<uint32_t>(90,resolution*2+5));
    auto *out=cJSON_CreateObject(),*columns=cJSON_CreateArray(),*rows=cJSON_CreateArray();
    cJSON_AddNumberToObject(out,"capacity",capacity);cJSON_AddNumberToObject(out,"interval_seconds",interval_ms/1000);
    auto *archive=history_store::status();bool persistent=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(archive,"ready"));
    cJSON_AddItemToObject(out,"archive",archive);cJSON_AddStringToObject(out,"storage",persistent?"littlefs+ram":"ram");cJSON_AddNumberToObject(out,"schema",1);
    cJSON_AddNumberToObject(out,"from",from);cJSON_AddNumberToObject(out,"to",to);cJSON_AddNumberToObject(out,"resolution_seconds",resolution);cJSON_AddNumberToObject(out,"gap_seconds",raw?90:std::max<uint32_t>(90,resolution*2+5));
    cJSON_AddBoolToObject(out,"export_raw",raw);
    for(auto key:{"time","native","mode"})cJSON_AddItemToArray(columns,cJSON_CreateString(key));
    for(auto &field:fields)cJSON_AddItemToArray(columns,cJSON_CreateString(field.key));
    cJSON_AddItemToArray(columns,cJSON_CreateString("gapBefore"));
    cJSON_AddItemToObject(out,"columns",columns);cJSON_AddItemToObject(out,"rows",rows);
    for(unsigned i=0;i<size;++i){const auto &sample=copy[i].sample;auto *row=cJSON_CreateArray();cJSON_AddItemToArray(rows,row);add(row,sample.time);add(row,sample.native);add(row,sample.mode);for(float value:sample.values)add(row,value);add(row,copy[i].gap_before);}
    heap_caps_free(copy);if(raw)cJSON_AddItemToObject(out,"settings_history",history_store::settings(from,to));return out;
}
namespace {
bool self_test() {
    unsigned total=0,passed=0;auto check=[&](bool ok){++total;if(ok)++passed;};
    auto *fixture=cJSON_Parse("{\"mode\":0,\"tempCO\":0,\"pumpCOWorks\":false,\"lambdaLevel\":73}");
    check(fixture&&read(fixture,fields[0])==0&&std::isnan(read(fixture,fields[2])));
    check(read(fixture,fields[17])==0&&std::abs(read(fixture,fields[16])-7.3)<0.00001);
    append(fixture,true,0,1000);check(count==0);
    append(fixture,true,1700000000,1000);check(count==1);
    append(fixture,true,1700000001,2000);check(count==1);
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(fixture,"mode"),2);
    append(fixture,true,1700000002,3000);check(count==2);
    auto *raw=snapshot(1700000000,1700000002,true);
    check(raw&&cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(raw,"export_raw"))&&cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(raw,"rows"))==2);
    cJSON_Delete(raw);check(snapshot(1700000000,1700003601,true)==nullptr);
    check(count==2&&samples[head].time==1700000000);
    check(std::isnan(samples[head].values[2]));
    clear();
    for(unsigned i=0;i<capacity+4;++i)append(fixture,true,1700000000+i*10,1000+i*10000);
    check(count==capacity&&samples[head].time==1700000040);
    check(samples[(head+count-1)%capacity].time==1700000000+(capacity+3)*10);
    append(fixture,true,1700000000,10000000);check(count==1&&head==0);
    append(fixture,false,1700000001,10000001);check(count==2);
    cJSON_Delete(fixture);clear();
    Point points[4]{};for(unsigned i=0;i<4;++i){points[i].sample.time=1700000000+i*60;points[i].sample.native=1;points[i].sample.values[0]=i;}
    check(history_data::reduce(points,4,1700000000,1700000239,120)==2&&points[0].sample.values[0]==1&&points[1].sample.values[0]==3);
    Point gaps[2]{};gaps[0].sample.time=1700000000;gaps[1].sample.time=1700000300;
    check(history_data::reduce(gaps,2,1700000000,1700000400,60)==2&&gaps[1].gap_before);
    check(history_data::step(1700000000,1700259199)==360);
    auto *days=static_cast<Point *>(heap_caps_calloc(4321,sizeof(Point),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(days){for(unsigned i=0;i<4321;++i){days[i].sample.time=1700000000+i*60;days[i].sample.native=1;}
        auto size=history_data::reduce(days,4321,1700000000,1700259200,history_data::step(1700000000,1700259200));
        check(size<=720&&size>700&&days[size-1].sample.time==1700259200);
        bool continuous=true;for(unsigned i=0;i<size;++i)continuous=continuous&&!days[i].gap_before;check(continuous);heap_caps_free(days);
    }else{check(false);check(false);}
    state::log("HISTORY SELF-TEST: %u/%u %s; offline fixtures, no boiler requests",passed,total,passed==total?"PASS":"FAIL");
    return total==passed;
}
}
}
