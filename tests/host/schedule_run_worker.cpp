// Exercise the production worker implementation with a fake boiler and durable
// storage. No serial, LAN or physical heater is contacted by this executable.
#include "cJSON.h"
#include "schedule_run.hpp"
#include "boiler_clock_codec.hpp"
#include <cassert>
#include <string>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <atomic>
#include <vector>
namespace test_worker {
#ifdef _WIN32
tm *gmtime_r(const time_t *t,tm *out){*out=*std::gmtime(t);return out;}
#endif
using J=cJSON;using nvs_handle_t=int;constexpr int ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,NVS_READWRITE=2;
struct Lock{};struct Owned{J *p;~Owned(){cJSON_Delete(p);}};
J *data=nullptr,*raw=nullptr;std::string error_text,uid="TEST-BOILER",selected_mode="rs485";
schedule_run::Record schedule_run_record{},stored{},staged{};bool schedule_run_pending=false;
schedule_run::Week actual{};std::atomic<bool> busy{false};bool native_identity=true;
uint64_t ticks=100000,native_identity_ms=99990;int64_t civil=0;int boiler_mode=5;
bool storage_ok=true,stored_present=false,clock_ok=true,reads_ok=true,send_ok=true,available=true,alarm=false;
std::vector<int> writes;
uint64_t now(){return ticks;}
J *get(const J *o,const char *k){return cJSON_GetObjectItemCaseSensitive(o,k);}
J *item(J *a,int i){return cJSON_GetArrayItem(a,i);}
const char *str(const J *j,const char *fallback=""){return cJSON_IsString(j)?j->valuestring:fallback;}
double num(const J *j,double fallback=NAN){return cJSON_IsNumber(j)?j->valuedouble:fallback;}
void put(J *o,const char *k,J *j){cJSON_DeleteItemFromObjectCaseSensitive(o,k);cJSON_AddItemToObject(o,k,j?j:cJSON_CreateNull());}
void copy(J *o,const char *k,const J *j){put(o,k,j?cJSON_Duplicate(j,true):nullptr);}
void text(J *o,const char *k,const std::string &s){put(o,k,cJSON_CreateString(s.c_str()));}
void number(J *o,const char *k,double n){put(o,k,cJSON_CreateNumber(n));}
void flag(J *o,const char *k,bool b){put(o,k,cJSON_CreateBool(b));}
bool fail(const char *e){error_text=e;return false;}
bool valid_schedule(const J *j){return cJSON_IsArray(j)&&cJSON_GetArraySize(j)==8;}
int nvs_open(const char*,int,nvs_handle_t *h){*h=1;return storage_ok?ESP_OK:9;}
int nvs_set_blob(int,const char*,const void *value,size_t n){assert(n==sizeof(staged));memcpy(&staged,value,n);return ESP_OK;}
int nvs_commit(int){if(!storage_ok)return 9;stored=staged;stored_present=true;return ESP_OK;}
int nvs_erase_key(int,const char*){staged={};staged.version=0;return storage_ok?ESP_OK:9;}
void nvs_close(int){}
void vTaskDelay(unsigned){}
void phase(const char*,const char*){}
unsigned pdMS_TO_TICKS(unsigned t){return t;}
namespace updates{bool installing(){return false;}}
namespace state{template<class... Args>void log(const char*,Args...){}}
namespace controller_profile{bool reference(const char *model,const char *module,const char *panel){return !strcmp(model,"ecoMAX860P-LZ TOUCH")&&!strcmp(module,"8.41.38.L0")&&!strcmp(panel,"8.30.28");}}
namespace transport{
struct Reply{bool sent=false;};bool enabled(){return available;}bool conflict(){return false;}
bool clock_sample(uint8_t *p,uint64_t &received){
    if(!clock_ok)return false;time_t t=civil;tm *v=gmtime(&t);memset(p,0,26);memcpy(p,"TIME",4);
    p[4]=v->tm_hour;p[5]=v->tm_min;p[6]=v->tm_sec;int year=v->tm_year+1900;
    p[7]=year&255;p[8]=year>>8;p[9]=v->tm_mon+1;p[10]=v->tm_mday;p[11]=v->tm_wday;received=ticks;return true;
}
bool request(int command,const uint8_t *p,size_t n,int,Reply &reply){
    assert(command==55&&n==46&&p[0]==1&&p[1]==3);writes.push_back(command);reply.sent=send_ok;
    if(send_ok){actual.meta[0]=p[2];actual.meta[1]=p[3];memcpy(actual.days,p+4,42);}return reply.sent;
}
}
namespace boiler{J *snapshot(){auto *j=cJSON_Parse("{\"fresh\":true,\"sensors\":{\"state\":5,\"pending_alerts\":0},\"outputs\":{\"alarm\":false}}");number(get(j,"sensors"),"state",boiler_mode);flag(get(j,"outputs"),"alarm",alarm);return j;}}
const char *control_guard(const J*,bool,bool id,bool enabled,bool){return id&&enabled?nullptr:"unavailable";}
bool rs_schedule_refresh(){return reads_ok;}
J *run_json(const schedule_run::Week &);
J *schedule_target(const char*){auto *j=cJSON_CreateObject();put(j,"before",run_json(actual));return j;}
bool inspect_generic(const char *id,J *&out){if(!reads_ok)return fail("no read");out=schedule_target(id);return true;}
bool rs_identify(){return native_identity;}
bool inspect_control(bool,bool,int &mode){mode=boiler_mode;return mode==0;}
bool encode_schedule(const J*,const J *week,uint8_t (&bytes)[46]){
    bytes[0]=1;bytes[1]=3;bytes[2]=num(item(item(const_cast<J*>(week),7),0));bytes[3]=num(item(item(const_cast<J*>(week),7),1));
    for(int d=0;d<7;++d)for(int b=0;b<6;++b)bytes[4+d*6+b]=num(item(item(const_cast<J*>(week),d),b));return true;
}
#include "panel_schedule_run.inc"
void reset(){
    cJSON_Delete(data);cJSON_Delete(raw);data=cJSON_CreateObject();raw=cJSON_Parse("{\"sysParams\":{\"controllerID\":\"ecoMAX860P-LZ TOUCH\",\"moduleASoftVer\":\"8.41.38.L0\",\"modulePanelSoftVer\":\"8.30.28\"}}");
    schedule_run_record={};schedule_run_pending=false;stored_present=false;storage_ok=clock_ok=reads_ok=send_ok=available=native_identity=true;alarm=false;boiler_mode=5;writes.clear();run_started_ms=0;next_run_check=0;busy=false;uid="TEST-BOILER";
    ticks=100000;native_identity_ms=ticks;actual={};actual.meta[0]=1;actual.meta[3]=20;
    for(unsigned d=0;d<7;++d)for(unsigned s=0;s<48;++s)if(s<3||s>=30)schedule_run::permit(actual,d*48+s);
    civil=boiler_clock_codec::civil(2026,11,9,12,16,49);
}
J *proposal(){auto *p=cJSON_CreateObject();int mode=-1;assert(run_prepare(p,mode));number(p,"before_mode",mode);return p;}
void begin(){Owned held{proposal()};int mode=-1;assert(run_begin(held.p,mode));assert(mode==5&&schedule_run_pending&&stored_present&&schedule_run::valid(stored));assert(writes.size()==1);}
}
int main(){using namespace test_worker;
    reset();Owned held{proposal()};int mode=-1;storage_ok=false;assert(!run_begin(held.p,mode)&&writes.empty()&&!schedule_run_pending);
    reset();alarm=true;Owned rejected{cJSON_CreateObject()};assert(!run_prepare(rejected.p,mode)&&writes.empty());
    reset();Owned changed_plan{proposal()};actual.days[3][1]^=1;assert(!run_begin(changed_plan.p,mode)&&writes.empty());
    reset();Owned passed_window{proposal()};civil+=3*3600;assert(!run_begin(passed_window.p,mode)&&writes.empty());
    reset();begin();const auto original=schedule_run_record.plan.before;const auto end=schedule_run_record.plan.end;
    // Reboot: reload durable state, no extra schedule write and no START replay.
    schedule_run_record=stored;run_started_ms=0;assert(run_reconcile());assert(writes.size()==1);
    civil=schedule_run_record.plan.resume;assert(run_reconcile());assert(writes.size()==2&&schedule_run::same(actual,original));
    assert(schedule_run_record.stage==schedule_run::Stage::watching);civil=end;assert(run_reconcile()&&!schedule_run_pending);
    reset();begin();assert(run_reconcile(true)&&!schedule_run_pending&&writes.size()==2);
    reset();begin();actual.days[4][0]^=1;auto changed=actual;assert(run_reconcile()&&!schedule_run_pending&&writes.size()==1&&schedule_run::same(actual,changed));
    reset();begin();clock_ok=false;assert(run_reconcile()&&!schedule_run_pending&&writes.size()==2);
    reset();begin();boiler_mode=0;run_started_ms=0;assert(run_reconcile()&&!schedule_run_pending&&writes.size()==2);
    reset();begin();boiler_mode=7;run_started_ms=0;assert(run_reconcile()&&schedule_run_pending&&writes.size()==1); // Normal thermostat-driven extinguishing is not a user STOP.
    reset();begin();uid="OTHER-BOILER";assert(!run_reconcile()&&writes.size()==1); // Never send old owner's week.
    reset();begin();civil=schedule_run_record.plan.resume;send_ok=false;assert(!run_reconcile()&&schedule_run_pending);assert(schedule_run_record.stage==schedule_run::Stage::restoring);
    send_ok=true;assert(run_reconcile()&&!schedule_run_pending&&schedule_run::same(actual,schedule_run_record.plan.before));
    reset();begin();schedule_run_record.stage=schedule_run::Stage::prepared;assert(run_reconcile()&&!schedule_run_pending&&writes.size()==2); // Interrupted/uncertain start.
    cJSON_Delete(data);cJSON_Delete(raw);
    puts("PASS production one-shot worker: durable-before-write, readback, restart, cancellation, external edit, stale clock, identity and failed restoration");
}
