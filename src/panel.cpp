#include "panel.hpp"
#include "panel_history.hpp"
#include "pellet.hpp"
#include "board.hpp"
#include "boiler.hpp"
#include "state.hpp"
#include "transport.hpp"
#include "protocol.hpp"
#include "thermostat_decoder.hpp"
#include "panel_catalog.hpp"
#include "econet_defaults.hpp"
#include "identity_guard.hpp"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "nvs.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <sys/time.h>
#include <atomic>
#include <utility>

namespace panel {
namespace {
using J = cJSON;
struct Config { char host[16]{}, user[64]{}, password[96]{}; } config;
SemaphoreHandle_t mutex;
J *data, *raw, *ticket;
std::string uid, error_text, queued_kind, queued_owner, ticket_owner;
J *queued_payload;
std::atomic<bool> busy{false};
uint64_t live_ms = 0, target_ms = 0, system_ms = 0, ticket_deadline = 0;
unsigned revision = 0, ticket_revision = 0;
std::string selected_mode="rs485";
std::atomic<bool> native_identity{false};
std::atomic<uint64_t> native_identity_ms{0};
plum::ThermostatSettings native_room;
uint64_t native_room_ms=0;
struct MixerSetpoint { int value=-1,low=-1,high=-1; uint64_t sample=0; } native_mixer;
uint64_t native_menu_ms=0,native_menu_config_ms=0,next_menu_poll=0,native_room_config_ms=0;
uint64_t native_menu_versions_ms=0;
uint64_t native_structure_ms=0,native_alarm_ms=0,next_alarm_poll=0,next_history_sample=0;
void fill_rs_curr(J *curr,const J *snapshot);
std::atomic<uint64_t> native_schedule_ms{0},econet_network_ms{0};
std::atomic<uint64_t> native_room_schedule_ms{0};
uint64_t next_room_schedule_poll=0;
unsigned room_schedule_failures=0;
uint64_t next_schedule_poll=0;
unsigned schedule_failures=0;
int native_structure_version=-1,last_pending_alerts=-1;
unsigned health_failures=0,alarm_failures=0;
uint64_t native_menu_row_ms[168]{};
std::string native_menu_error;
uint64_t health_ms=0, next_health=0;
struct Lock { Lock() { xSemaphoreTake(mutex, portMAX_DELAY); } ~Lock() { xSemaphoreGive(mutex); } };
struct Owned { J *p; ~Owned() { cJSON_Delete(p); } };
uint64_t now() { return esp_timer_get_time() / 1000; }
double epoch() { return static_cast<double>(time(nullptr)); }
J *get(const J *o, const char *k) { return cJSON_GetObjectItemCaseSensitive(o, k); }
const char *str(const J *o, const char *fallback = "") { return cJSON_IsString(o) ? o->valuestring : fallback; }
double num(const J *o, double fallback = NAN) { return cJSON_IsNumber(o) ? o->valuedouble : fallback; }
void put(J *o, const char *k, J *v) { if(!o) { cJSON_Delete(v); return; } cJSON_DeleteItemFromObjectCaseSensitive(o, k); cJSON_AddItemToObject(o, k, v ? v : cJSON_CreateNull()); }
void copy(J *o, const char *k, const J *v) { put(o, k, v ? cJSON_Duplicate(v, true) : nullptr); }
void text(J *o, const char *k, const std::string &v) { put(o, k, cJSON_CreateString(v.c_str())); }
void number(J *o, const char *k, double v) { put(o, k, std::isfinite(v) ? cJSON_CreateNumber(v) : cJSON_CreateNull()); }
void flag(J *o, const char *k, bool v) { put(o, k, cJSON_CreateBool(v)); }
J *body(const char *endpoint) { return get(get(raw, endpoint), "data"); }
J *item(J *array, int index) { return cJSON_GetArrayItem(array, index); }
bool fail(const char *message) { error_text = message; return false; }
const char *network_keys[]={"172","173","174","175","176","177","178","179","180","181","182","184"};
std::string timestamp() { char buffer[40]; time_t t = time(nullptr); struct tm utc{}; gmtime_r(&t, &utc); strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc); return buffer; }
std::string random_token() { uint8_t b[16]; esp_fill_random(b, sizeof(b)); char s[33]; for (unsigned i=0;i<16;++i) snprintf(s+2*i,3,"%02x",b[i]); return s; }
std::string encode(const std::string &s) { std::string out; char b[4]; for (unsigned char c : s) { if ((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='-'||c=='_') out+=c; else { snprintf(b,sizeof(b),"%%%02X",c); out+=b; } } return out; }
bool private_ip(const char *s) { unsigned a,b,c,d; char tail; if (sscanf(s,"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)!=4 || a>255||b>255||c>255||d>255) return false; char canonical[16]; snprintf(canonical,sizeof(canonical),"%u.%u.%u.%u",a,b,c,d); return !strcmp(s,canonical) && (a==10 || (a==172&&b>=16&&b<=31) || (a==192&&b==168)); }
struct Response { std::string bytes; bool overflow=false; };
esp_err_t collect(esp_http_client_event_t *e) {
    auto &r=*static_cast<Response *>(e->user_data);
    if (e->event_id==HTTP_EVENT_ON_DATA && e->data_len>0) {
        if (r.bytes.size()+e->data_len>512*1024) { r.overflow=true; return ESP_FAIL; }
        r.bytes.append(static_cast<const char *>(e->data),e->data_len);
    }
    return ESP_OK;
}
// Only the worker calls this client. A failed write is never retried here.
J *request(const char *endpoint, const std::string &query="", bool write=false, unsigned timeout=4500) {
    std::string path="http://"+std::string(config.host)+"/econet/"+endpoint;
    std::string params=query;
    if (!write && strcmp(endpoint,"sysParams") && strcmp(endpoint,"regParams") && strcmp(endpoint,"regParamsData") && strcmp(endpoint,"editParams") && !uid.empty()) params+=(params.empty()?"":"&")+std::string("uid=")+encode(uid);
    if (!params.empty()) path+='?'+params;
    Response r;
    esp_http_client_config_t cfg{}; cfg.url=path.c_str(); cfg.timeout_ms=timeout;
    cfg.event_handler=collect; cfg.user_data=&r; cfg.disable_auto_redirect=true; cfg.max_authorization_retries=-1;
    auto client=esp_http_client_init(&cfg);
    if (!client) { fail("Brak pamieci klienta HTTP"); return nullptr; }
    std::string plain=std::string(config.user)+":"+config.password; char auth[256]="Basic "; size_t len=0;
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char *>(auth+6),sizeof(auth)-7,&len,reinterpret_cast<const unsigned char *>(plain.data()),plain.size())) { esp_http_client_cleanup(client); fail("Niepoprawna konfiguracja logowania"); return nullptr; }
    auth[len+6]=0; esp_http_client_set_header(client,"Authorization",auth);
    esp_http_client_set_header(client,"Cache-Control","no-cache"); esp_http_client_set_header(client,"Pragma","no-cache");
    auto started=now(); auto result=esp_http_client_perform(client); int status=esp_http_client_get_status_code(client);
    if(result!=ESP_OK) state::log("ecoNET %s %s: %s, errno=%d, HTTP=%d",write?"write":"read",endpoint,esp_err_to_name(result),esp_http_client_get_errno(client),status);
    esp_http_client_cleanup(client);
    J *value=nullptr;
    if (result==ESP_OK && status==200 && !r.overflow) {
        if (r.bytes.size()>=3 && static_cast<unsigned char>(r.bytes[0])==239) r.bytes.erase(0,3);
        value=cJSON_ParseWithLength(r.bytes.data(),r.bytes.size());
        if (!cJSON_IsObject(value) || get(value,"error")) { cJSON_Delete(value); value=nullptr; fail("ecoNET zwrocilo niepoprawne dane lub blad API"); }
    } else fail(status==401?"ecoNET: niepoprawny login lub haslo":"Brak poprawnej odpowiedzi lokalnego ecoNET");
    { Lock lock; auto *health=cJSON_CreateObject(); text(health,"host",config.host);
      flag(health,"available",status>0); flag(health,"api_ok",value!=nullptr); number(health,"http_status",status);
      number(health,"latency_ms",now()-started); text(health,"checked",timestamp());
      if(!value) text(health,"error",error_text);
      put(data,"econet_health",health); health_ms=now();
      if(value&&!write&&!strcmp(endpoint,"regParamsData")) {
          auto *network=cJSON_CreateObject(),*values=cJSON_CreateObject();
          for(auto key:network_keys)copy(values,key,get(get(value,"data"),key));
          put(network,"values",values); text(network,"source","ecoNET HTTP regParamsData"); number(network,"read_epoch",epoch());
          put(data,"econet_network",network); econet_network_ms=now();
      } }
    if (!write) {
        Lock lock; auto *record=cJSON_CreateObject(); text(record,"state",value?"ok":"error");
        number(record,"latency_ms",now()-started); text(record,"checked",timestamp());
        if (!value) text(record,"error",error_text); else { auto *p=get(value,"data"); if (!p) p=get(value,"curr"); if (!p) p=value; number(record,"count",cJSON_GetArraySize(p)); }
        put(get(data,"endpoints"),endpoint,record);
        if (!strcmp(endpoint,"regParams")) number(data,"latency_ms",now()-started);
    }
    return value;
}
bool read(const char *endpoint, bool language=false) {
    auto *value=request(endpoint,language?"lang=pl":"");
    if(!value) { vTaskDelay(pdMS_TO_TICKS(150)); value=request(endpoint,language?"lang=pl":""); }
    if (!value) return false;
    put(raw,endpoint,value); return true;
}
bool remember_identity(const char *identity,bool supported_model=true) {
    auto action=identity_guard::decide(uid.c_str(),identity,supported_model);
    if(action==identity_guard::Action::reject) return fail("Niezgodna tozsamosc lub model kotla. Zapis zablokowany");
    if(action==identity_guard::Action::enroll) {
        nvs_handle_t h; if(nvs_open("eco_panel",NVS_READWRITE,&h)!=ESP_OK) return fail("Nie utrwalono tozsamosci kotla");
        auto result=nvs_set_str(h,"identity",identity); if(result==ESP_OK) result=nvs_commit(h); nvs_close(h);
        if(result!=ESP_OK) return fail("Nie utrwalono tozsamosci kotla");
        uid=identity;
    }
    return true;
}
bool identify() {
    if (!read("sysParams")) return false;
    auto *sys=get(raw,"sysParams"); const char *identity=str(get(sys,"uid"));
    if (!*identity || strlen(identity)>160 || !cJSON_IsString(get(sys,"controllerID"))) return fail("Brak identyfikacji sterownika");
    if(!remember_identity(identity)) return false;
    { Lock lock; auto *s=cJSON_CreateObject(); for (auto k:{"controllerID","softVer","moduleASoftVer","modulePanelSoftVer","remoteMenu","schemaID"}) copy(s,k,get(sys,k)); put(data,"system",s);
      copy(data,"alarms",get(sys,"alarms")); copy(data,"schedules",get(sys,"schedules"));
      auto *tiles=cJSON_CreateArray(); J *t; cJSON_ArrayForEach(t,get(sys,"tiles")) { auto *v=cJSON_CreateObject(); for(auto k:{"type_","memberName_","setMemberName_","seqNo_"}) if(get(t,k)) copy(v,k,get(t,k)); cJSON_AddItemToArray(tiles,v); }
      put(data,"tiles",tiles); system_ms=now(); number(data,"system_read_epoch",epoch()); }
    return true;
}
J *enum_value(const J *meta) {
    if (num(get(meta,"unit"))!=31) return cJSON_CreateNull();
    auto *e=item(body("rmParamsEnums"),static_cast<int>(num(get(meta,"offset"),-1)));
    double position=num(get(meta,"value"))-num(get(e,"first"),0);
    if (!std::isfinite(position)||position<0||floor(position)!=position) return cJSON_CreateNull();
    auto *v=item(get(e,"values"),static_cast<int>(position)); return v?cJSON_Duplicate(v,true):cJSON_CreateNull();
}
struct Definition { const char *id, *name, *key, *schedule; int index,low,high; double step; bool legacy,native; const char *measured; int position; };
const Definition definitions[]={
    {"1280","Temperatura zadana kot\u0142a","tempCOSet","boilerTZ",0,70,88,1,false,true,"tempCO",0},
    {"1281","Temperatura zadana CWU","tempCWUSet","cwuTZ",47,35,65,1,false,true,"tempCWU",0},
    {"1287","Temperatura zadana mieszacza 1","mixerSetTemp1","mixer1TZ",62,40,65,1,false,true,"mixerTemp1",0},
    {"STER_TEMP_DAY_1","Temperatura pokojowa dzienna","ecoSterSetTemp1","",-1,10,35,0.1,true,true,"ecoSterTemp1",2},
    {"STER_TEMP_NIGHT_1","Temperatura pokojowa nocna","ecoSterSetTemp1","",-1,10,35,0.1,true,true,"ecoSterTemp1",3},
    {"STER_TEMP_SET_PARTY_1","Temperatura zadana dla trybu Party","ecoSterSetTemp1","",-1,10,35,0.1,true,true,"ecoSterTemp1",5},
    {"STER_TEMP_SET_SUMMER_1","Temperatura zadana dla trybu Wakacje","ecoSterSetTemp1","",-1,10,35,0.1,true,true,"ecoSterTemp1",6}
};
const Definition *definition(const char *id) { for(auto &d:definitions) if(!strcmp(id,d.id)) return &d; return nullptr; }
bool equal(double a,double b) { return std::isfinite(a)&&std::isfinite(b)&&fabs(a-b)<0.0001; }
bool aligned(double value,const Definition *d) { return d&&std::isfinite(value)&&equal(value/d->step,round(value/d->step)); }
std::string wire_value(double value) { char out[32]; snprintf(out,sizeof(out),"%.9g",value); return out; }
void menu_write(J *row,const Definition &d) { text(row,"write_id",d.id); flag(row,"write_enabled",selected_mode!="rs485"||d.native); }
double scalar(const J *v) { return cJSON_IsBool(v)?(cJSON_IsTrue(v)?1:0):num(v); }
double room_mode(const J *value) {
    double raw_value=num(value); if(!std::isfinite(raw_value)||raw_value<0||raw_value>255||floor(raw_value)!=raw_value) return NAN;
    unsigned mode=static_cast<unsigned>(raw_value)&0x7f; return mode<=7?mode:NAN;
}
const char *room_modes[]={"Harmonogram","Ekonomiczny","Komfortowy","Wyjscie z domu","Wietrzenie","Party","Wakacje","Przeciwzamrozeniowy"};
bool generic_id(const char *id) { return !strncmp(id,"rm:",3)||!strncmp(id,"legacy:",7)||!strncmp(id,"schedule:",9); }
J *generic_target(const char *id);
J *rs_target(const char *id);
J *rs_menu_temperature_target(const char *id);
// Access is inherited from every parent section, not only the parameter row.
bool menu_access(int wanted) {
    struct Access { bool locked; double pass; }; Access sections[256]{};
    unsigned count=0,parent=0,matches=0; bool allowed=true; J *row; int position=0;
    cJSON_ArrayForEach(row,body("rmStructure")) {
        int type=static_cast<int>(num(get(row,"type"),-1));
        if(type==7&&position>2) ++parent;
        else if(type==0||type==2||type==4||type==5) {
            if(count>=256||parent>=256) return false;
            sections[count++]={sections[parent].locked||!cJSON_IsFalse(get(row,"lock")),std::max(sections[parent].pass,num(get(row,"pass_index"),99))};
        } else if(type==1&&num(get(row,"index"))==wanted) {
            ++matches; if(parent>=count||sections[parent].locked||sections[parent].pass>1||!cJSON_IsFalse(get(row,"lock"))||num(get(row,"pass_index"),99)>1) allowed=false;
        }
        ++position;
    }
    return matches==1&&allowed;
}
bool menu_versions_match(const J *details) {
    if(num(get(details,"command"))!=0||num(get(details,"minor"))!=0||num(get(details,"major"))!=1) return false;
    // Bound labels, enumeration and inherited access to the captured ecoNET catalogue.
    const unsigned expected[][2]={{2,7672},{3,51616},{6,13175},{7,7227},{8,15074},{13,50115},{14,373}};
    for(auto &pair:expected) {
        unsigned count=0; J *row; cJSON_ArrayForEach(row,get(details,"commands")) if(num(get(row,"command"))==pair[0]) {
            if(num(get(row,"version"))!=pair[1]) return false;
            ++count;
        }
        if(count!=1) return false;
    }
    unsigned structures=0; J *row;
    cJSON_ArrayForEach(row,get(details,"commands")) if(num(get(row,"command"))==9) {
        double version=num(get(row,"version")); if(!std::isfinite(version)||version<0||version>65535||floor(version)!=version)return false;
        ++structures;
    }
    return structures==1;
}
int structure_version(const J *details) {
    J *row; cJSON_ArrayForEach(row,get(details,"commands")) if(num(get(row,"command"))==9)return static_cast<int>(num(get(row,"version"),-1));
    return -1;
}
bool menu_structure_valid(const J *structure) {
    if(!cJSON_IsArray(structure)||!cJSON_GetArraySize(structure)||cJSON_GetArraySize(structure)>256)return false;
    unsigned sections=0,parent=0,position=0; J *row;
    cJSON_ArrayForEach(row,structure) {
        int kind=static_cast<int>(num(get(row,"type"),-1)),index=static_cast<int>(num(get(row,"index"),-1));
        double pass=num(get(row,"pass_index"));
        if(kind<0||kind>7||kind==6||index<0||index>255||!cJSON_IsBool(get(row,"lock"))||!std::isfinite(pass)||pass<0||pass>7||floor(pass)!=pass)return false;
        if(kind==7) { if(position>2)++parent; if(parent>sections)return false; }
        else if(kind==0||kind==2||kind==4||kind==5) {
            if(index>=cJSON_GetArraySize(body("rmCatsNames")))return false;
            if(!sections) { if(kind!=0||index!=0)return false; }
            else if(parent>=sections)return false;
            ++sections;
        } else if(!sections||parent>=sections||(kind==1&&index>=168))return false;
        ++position;
    }
    return sections>0;
}
const char *room_wire_keys[]={"STER_MODE_1","STER_TEMP_SET_PARTY_1","STER_TEMP_SET_SUMMER_1","STER_CORRECTION_1","STER_TIMER_EXIT_1","STER_TIMER_VENT_1","STER_TIMER_PARTY_1","STER_TIMER_HOLIDAY_1","STER_HYSTEREZE_1","STER_TEMP_DAY_1","STER_TEMP_NIGHT_1","STER_TEMP_ANTIFREEZ_1"};
int room_wire_index(const char *id) {
    if(strncmp(id,"legacy:",7))return -1;
    for(unsigned i=0;i<12;++i)if(!strcmp(id+7,room_wire_keys[i]))return i+1;
    return -1;
}
J *rs_room_parameter_target(const char *id,bool match_sensor=true) {
    int wire=room_wire_index(id); if(wire<0)return nullptr;
    if(selected_mode!="rs485"||!transport::enabled()||transport::conflict()||!native_identity||now()-native_identity_ms>60000||
       !native_room_ms||now()-native_room_ms>15000||native_room.slots!=3||!native_room_config_ms||now()-native_room_config_ms>120000)return nullptr;
    Owned sensor{boiler::snapshot()}; auto *configuration=item(body("rsThermostatConfig"),wire);
    const unsigned units[]={31,1,1,1,4,3,4,5,1,1,1,1};
    double mult=wire==1?19:(wire>=5&&wire<=8?1:0.1),offset=wire==4?100:0;
    if(!cJSON_IsTrue(get(sensor.p,"fresh"))||strcmp(str(get(get(sensor.p,"modules"),"ecoster")),"1.21.38")||
       strcmp(str(get(get(sensor.p,"modules"),"module_a")),"8.41.38.L0")||
       num(get(configuration,"index"))!=wire||num(get(configuration,"unit"))!=units[wire-1]||!equal(num(get(configuration,"mult")),mult)||num(get(configuration,"offset"))!=offset)return nullptr;
    const auto &m=native_room.values[0][0]; J *room=nullptr,*row;
    cJSON_ArrayForEach(row,get(sensor.p,"thermostats"))if(num(get(row,"index"))==0)room=row;
    if(!room||!m.present||m.low!=0||m.high!=7||(m.value&127)>7||(match_sensor&&num(get(room,"state"))!=m.value))return nullptr;
    const auto &p=native_room.values[0][wire-1];
    const unsigned lows[]={0,100,100,60,1,1,1,1,2,100,100,50},highs[]={7,350,350,140,60,60,60,60,50,350,350,300};
    if(!p.present||p.low<lows[wire-1]||p.high>highs[wire-1])return nullptr;
    auto *v=cJSON_CreateObject(); text(v,"id",id); text(v,"name",room_wire_keys[wire-1]);
    cJSON_ArrayForEach(row,get(data,"menu"))if(!strcmp(str(get(row,"index")),room_wire_keys[wire-1]))text(v,"name",str(get(row,"name")));
    number(v,"before",wire==1?p.value&127:(p.value-offset)*mult); number(v,"min",wire==1?p.low:(p.low-offset)*mult); number(v,"max",wire==1?p.high:(p.high-offset)*mult); number(v,"step",wire==1?1:mult);
    text(v,"unit",wire==1?"":wire==8?"dni":wire==5||wire==7?"h.":wire==6?"min.":"\u00b0C"); text(v,"value_kind",wire==1?"enum":"number"); flag(v,"generic",true); flag(v,"cached",false); flag(v,"risk",false);
    text(v,"schema","RS485_220_37_ECOSTER_1.21.38"); text(v,"wire_key",room_wire_keys[wire-1]); number(v,"wire_index",wire); number(v,"wire_width",plum::thermostat_sizes[wire-1]); number(v,"menu_index",-1);
    number(v,"wire_unit",units[wire-1]); number(v,"wire_mult",wire==1?1:mult); number(v,"wire_offset",offset); number(v,"profile_mode",m.value&127); number(v,"profile_raw",m.value);
    number(v,"parameter_sample_ms",native_room_ms); text(v,"observed_key",wire==1?"ecoSterMode1":"");
    auto *options=cJSON_CreateArray(); if(wire==1)for(unsigned i=0;i<8;++i) {auto *o=cJSON_CreateObject();number(o,"value",i);text(o,"label",room_modes[i]);cJSON_AddItemToArray(options,o);} put(v,"options",options); return v;
}
J *generic_target(const char *id) {
    bool direct=selected_mode=="rs485";
    if(direct&&room_wire_index(id)>0)return rs_room_parameter_target(id);
    if(direct&&(!native_identity||!transport::enabled()||transport::conflict()||!native_menu_config_ms||now()-native_menu_config_ms>65000||!native_menu_versions_ms||now()-native_menu_versions_ms>65000||!native_structure_ms||now()-native_structure_ms>65000)) return nullptr;
    auto *sys=get(raw,"sysParams"), *curr=get(get(raw,"regParams"),"curr");
    if(strcmp(str(get(sys,"controllerID")),"ecoMAX860P-LZ TOUCH")||strcmp(str(get(sys,"moduleASoftVer")),"8.41.38.L0")) return nullptr;
    bool legacy=!strncmp(id,"legacy:",7); int index=-1; J *meta=nullptr; std::string key,name,unit;
    if(legacy) {
        if(direct) return nullptr;
        key=id+7; auto at=key.rfind('_');
        if(at==std::string::npos||key.size()!=at+2||key.back()<'1'||key.back()>'8') return nullptr;
        const char *prefixes[]={"STER_MODE","STER_TEMP_DAY","STER_TEMP_NIGHT","STER_TEMP_ANTIFREEZ","STER_TEMP_SET_PARTY","STER_TEMP_SET_SUMMER","STER_HYSTEREZE","STER_CORRECTION","STER_TIMER_EXIT","STER_TIMER_VENT","STER_TIMER_PARTY","STER_TIMER_HOLIDAY"};
        int position=0; for(unsigned i=0;i<12;++i) if(key.substr(0,at)==prefixes[i]) position=i+1; if(!position) return nullptr;
        if(!std::isfinite(num(get(curr,("ecoSterTemp"+key.substr(at+1)).c_str())))) return nullptr;
        meta=get(body("editParams"),key.c_str()); name=str(get(meta,"name"),key.c_str());
        if(num(get(meta,"sec"))!=36||num(get(meta,"pos"))!=position) return nullptr;
        // Names are installed by build_menu; this lookup never grants permission.
        J *r; cJSON_ArrayForEach(r,get(data,"menu")) if(key==str(get(r,"index"))) { name=str(get(r,"name")); unit=str(get(r,"unit_name")); break; }
    } else {
        if(strncmp(id,"rm:",3)) return nullptr;
        char tail; if(sscanf(id+3,"%d%c",&index,&tail)!=1||index<0||index>=cJSON_GetArraySize(body("rmParamsData"))) return nullptr;
        char canonical[24]; snprintf(canonical,sizeof(canonical),"rm:%d",index); if(strcmp(id,canonical)||!menu_access(index)) return nullptr;
        if(direct&&(index>=168||!native_menu_row_ms[index]||now()-native_menu_row_ms[index]>20000)) return nullptr;
        meta=item(body("rmParamsData"),index); name=str(item(body("rmParamsNames"),index));
        unit=str(item(body("rmParamsUnitsNames"),static_cast<int>(num(get(meta,"unit"),-1)))); key=std::to_string(index);
    }
    if(name.empty()||!cJSON_IsTrue(get(meta,"edit"))) return nullptr;
    double low=scalar(get(meta,"minv")),high=scalar(get(meta,"maxv")),base=scalar(get(meta,"value"));
    bool discrete=num(get(meta,"unit"))==31,boolean=cJSON_IsBool(get(meta,"value"));
    double step=discrete||boolean?1:num(get(meta,"mult"));
    if(!std::isfinite(low)||!std::isfinite(high)||!std::isfinite(base)||!std::isfinite(step)||step<=0||step>10000||low>high||base<low||base>high) return nullptr;
    auto *options=cJSON_CreateArray();
    if(discrete) {
        if(floor(low)!=low||floor(high)!=high||high-low>64) { cJSON_Delete(options); return nullptr; }
        auto *en=legacy?nullptr:item(body("rmParamsEnums"),static_cast<int>(num(get(meta,"offset"),-1)));
        if(legacy&&(key.substr(0,key.rfind('_'))!="STER_MODE"||num(get(meta,"mult"))!=19||low<0||high>7)) { cJSON_Delete(options); return nullptr; }
        for(int v=static_cast<int>(low);v<=high;++v) {
            const char *label=legacy?room_modes[v]:str(item(get(en,"values"),v-static_cast<int>(num(get(en,"first"),0))));
            if(!*label) continue;
            auto *o=cJSON_CreateObject(); number(o,"value",v); text(o,"label",label); cJSON_AddItemToArray(options,o);
        }
        bool found=false; J *o; cJSON_ArrayForEach(o,options) if(num(get(o,"value"))==base) found=true;
        if(!found) { cJSON_Delete(options); return nullptr; }
    }
    auto *v=cJSON_CreateObject(); text(v,"id",id); text(v,"name",name); text(v,"unit",unit); number(v,"before",base); number(v,"min",low); number(v,"max",high); number(v,"step",step); put(v,"options",options);
    text(v,"endpoint",legacy?"newParam":"rmNewParam"); text(v,"wire_key",key); text(v,"value_kind",boolean?"boolean":discrete?"enum":"number"); flag(v,"generic",true); flag(v,"risk",!legacy); flag(v,"cached",legacy); number(v,"menu_index",index);
    if(direct) { number(v,"wire_unit",num(get(meta,"unit"))); number(v,"wire_mult",num(get(meta,"mult"))); number(v,"wire_offset",num(get(meta,"offset"))); }
    if(legacy) {
        std::string slot=key.substr(key.rfind('_')+1),prefix=key.substr(0,key.rfind('_'));
        double mode=room_mode(get(curr,("ecoSterMode"+slot).c_str())); if(!std::isfinite(mode)) { cJSON_Delete(v); return nullptr; } number(v,"profile_mode",mode); copy(v,"profile_raw",get(curr,("ecoSterMode"+slot).c_str()));
        bool active=prefix=="STER_MODE"||(prefix=="STER_TEMP_DAY"&&mode==2)||(prefix=="STER_TEMP_NIGHT"&&mode==1)||(prefix=="STER_TEMP_SET_PARTY"&&mode==5)||(prefix=="STER_TEMP_SET_SUMMER"&&mode==6)||(prefix=="STER_TEMP_ANTIFREEZ"&&mode==7);
        if(active) { double observed=prefix=="STER_MODE"?mode:num(get(curr,("ecoSterSetTemp"+slot).c_str())); if(!std::isfinite(observed)||observed<low||observed>high) { cJSON_Delete(v); return nullptr; } number(v,"before",round(observed/step)*step); flag(v,"cached",false); }
        text(v,"observed_key",prefix=="STER_MODE"?"ecoSterMode"+slot:active?"ecoSterSetTemp"+slot:"");
    }
    // Compare type, enumeration and wire routing again immediately before dispatch.
    auto *schema=cJSON_Duplicate(meta,true); cJSON_DeleteItemFromObjectCaseSensitive(schema,"value"); char *fingerprint=cJSON_PrintUnformatted(schema); text(v,"schema",fingerprint?fingerprint:""); cJSON_free(fingerprint); cJSON_Delete(schema);
    return v;
}
bool generic_value(const J *t,double value) {
    double step=num(get(t,"step")),low=num(get(t,"min")),high=num(get(t,"max"));
    if(!std::isfinite(value)||!std::isfinite(step)||step<=0||value<low||value>high||!equal((value-low)/step,round((value-low)/step))) return false;
    if(!strcmp(str(get(t,"value_kind")),"boolean")&&value!=0&&value!=1) return false;
    auto *options=get(t,"options"); if(cJSON_GetArraySize(options)) { J *o; cJSON_ArrayForEach(o,options) if(equal(value,num(get(o,"value")))) return true; return false; }
    return true;
}
bool encode_menu_value(const J *target,double value,uint8_t &encoded) {
    if(!generic_value(target,value)) return false;
    double unit=num(get(target,"wire_unit")),mult=num(get(target,"wire_mult")),offset=num(get(target,"wire_offset"));
    if(!std::isfinite(unit)||!std::isfinite(mult)||!std::isfinite(offset)||unit<0||unit>31||(unit!=31&&mult<=0)) return false;
    double wire=unit==31?value:value/mult+offset;
    if(!std::isfinite(wire)||wire<0||wire>255||!equal(wire,round(wire))) return false;
    encoded=static_cast<uint8_t>(std::lround(wire)); return true;
}
bool valid_schedule(const J *a) {
    if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)!=8) return false;
    for(int day=0;day<7;++day) { auto *r=item(const_cast<J*>(a),day); if(!cJSON_IsArray(r)||cJSON_GetArraySize(r)!=6) return false; J *v; cJSON_ArrayForEach(v,r) { double n=num(v); if(!std::isfinite(n)||n<0||n>255||floor(n)!=n) return false; } }
    auto *m=item(const_cast<J*>(a),7); if(!cJSON_IsArray(m)||cJSON_GetArraySize(m)!=4) return false;
    double enabled=num(item(m,0)),reduction=num(item(m,1)),low=num(item(m,2)),high=num(item(m,3));
    return (enabled==0||enabled==1)&&std::isfinite(reduction)&&std::isfinite(low)&&std::isfinite(high)&&low>=0&&high<=100&&low<=high&&reduction>=low&&reduction<=high&&floor(reduction)==reduction&&floor(low)==low&&floor(high)==high;
}
const char *rs_schedule_keys[]={"boilerTZ","cwuTZ","circPumpTZ","boilerWorkTZ","boilerCleanTZ","exchangerCleanTZ","mixer1TZ","mixer2TZ","mixer3TZ","mixer4TZ"};
int rs_schedule_index(const char *key) {
    for(unsigned i=0;i<10;++i)if(!strcmp(key,rs_schedule_keys[i]))return i;
    if(!strcmp(key,"thermostat1TZ"))return 16;
    return -1;
}
bool rs_schedule_refresh() {
    transport::Reply reply; std::string error;
    if(!transport::request_readonly(54,nullptr,0,182,reply))error="Brak odpowiedzi harmonogramow RS485";
    Owned parsed{error.empty()?protocol::describe(reply.bytes,reply.length):nullptr}; auto *details=get(parsed.p,"data");
    if(error.empty()&&(strcmp(str(get(parsed.p,"state")),"decoded")||num(get(details,"marker"))!=16||num(get(details,"start"))!=1))error="Niezweryfikowany format harmonogramow RS485";
    if(!error.empty()) { Lock lock; auto *status=get(data,"schedule_status"); if(!cJSON_IsObject(status)){status=cJSON_CreateObject();put(data,"schedule_status",status);} text(status,"error",error); return fail(error.c_str()); }
    auto *schedules=cJSON_CreateObject(),*weeks=cJSON_CreateObject(),*indices=cJSON_CreateObject(); J *row;
    copy(weeks,"thermostat1TZ",get(get(body("rsSchedules"),"ecomaxSchedules"),"thermostat1TZ"));
    if(!get(get(body("rsSchedules"),"ecomaxSchedules"),"thermostat1TZ"))cJSON_DeleteItemFromObjectCaseSensitive(weeks,"thermostat1TZ");
    if(cJSON_IsNumber(get(get(data,"schedule_write_indices"),"thermostat1TZ")))copy(indices,"thermostat1TZ",get(get(data,"schedule_write_indices"),"thermostat1TZ"));
    cJSON_ArrayForEach(row,get(details,"schedules")) {
        int index=static_cast<int>(num(get(row,"index"))); std::string key=index<10?rs_schedule_keys[index]:"schedule_"+std::to_string(index);
        copy(weeks,key.c_str(),get(row,"week"));
        if(index<10&&cJSON_IsTrue(get(row,"parameter_present"))&&valid_schedule(get(row,"week")))number(indices,key.c_str(),index);
    }
    put(schedules,"ecomaxSchedules",weeks);
    auto *wrapper=cJSON_CreateObject(); copy(wrapper,"data",schedules); put(raw,"rsSchedules",wrapper);
    { Lock lock; copy(data,"schedules",schedules); put(data,"schedule_write_indices",indices); native_schedule_ms=now();
      auto *status=cJSON_CreateObject(); text(status,"source","RS485 54/182"); number(status,"read_epoch",epoch()); number(status,"count",num(get(details,"count"))); put(status,"error",nullptr); put(data,"schedule_status",status); }
    cJSON_Delete(schedules); return true;
}
bool rs_room_schedule_refresh() {
    transport::Reply reply; std::string error;
    Owned sensor{boiler::snapshot()};
    if(!cJSON_IsTrue(get(sensor.p,"fresh"))||strcmp(str(get(get(sensor.p,"modules"),"ecoster")),"1.21.38"))error="Brak zweryfikowanego ecoSTER";
    if(error.empty()&&!transport::read_thermostat_schedule(reply)) {
        vTaskDelay(pdMS_TO_TICKS(150));
        if(!transport::read_thermostat_schedule(reply))error="Brak odpowiedzi ecoSTER 88/216";
    }
    Owned parsed{error.empty()?protocol::describe(reply.bytes,reply.length):nullptr}; auto *details=get(parsed.p,"data");
    if(error.empty()&&(strcmp(str(get(parsed.p,"state")),"decoded")||num(get(details,"marker"))!=16||num(get(details,"start"))!=1))error="Niezweryfikowany format harmonogramu ecoSTER";
    auto *record=get(details,"schedules"),*found=static_cast<J*>(nullptr); J *row;
    cJSON_ArrayForEach(row,record)if(num(get(row,"index"))==16)found=row;
    if(error.empty()&&(!found||!valid_schedule(get(found,"week"))))error="Brak harmonogramu ecoSTER T1 w odpowiedzi";
    if(!error.empty()) { Lock lock; auto *status=get(data,"thermostat_schedule_status"); if(!status){status=cJSON_CreateObject();put(data,"thermostat_schedule_status",status);} text(status,"error",error); return fail(error.c_str()); }
    auto *schedules=cJSON_Duplicate(body("rsSchedules"),true); if(!schedules)schedules=cJSON_CreateObject();
    auto *weeks=get(schedules,"ecomaxSchedules"); if(!weeks){weeks=cJSON_CreateObject();put(schedules,"ecomaxSchedules",weeks);}
    copy(weeks,"thermostat1TZ",get(found,"week"));
    auto *wrapper=cJSON_CreateObject(); copy(wrapper,"data",schedules); put(raw,"rsSchedules",wrapper);
    auto *entry=cJSON_CreateObject(); copy(entry,"data",found); put(raw,"rsThermostatSchedule",entry);
    { Lock lock; copy(data,"schedules",schedules); native_room_schedule_ms=now();
      auto *status=cJSON_CreateObject(); text(status,"source","ecoSTER 81 / RS485 88/216"); number(status,"read_epoch",epoch()); number(status,"count",1); put(status,"error",nullptr); put(data,"thermostat_schedule_status",status);
      auto *indices=get(data,"schedule_write_indices");
      if(!cJSON_IsObject(indices)){indices=cJSON_CreateObject();put(data,"schedule_write_indices",indices);}
      auto *meta=item(get(found,"week"),7); double parameter=num(get(found,"parameter_raw"));
      double normalized=parameter==255?0:parameter;
      if(((parameter>=0&&parameter<=100&&floor(parameter)==parameter)||parameter==255)&&num(item(meta,1))==normalized&&num(item(meta,2))==normalized&&num(item(meta,3))==normalized)number(indices,"thermostat1TZ",16);
      else cJSON_DeleteItemFromObjectCaseSensitive(indices,"thermostat1TZ"); }
    cJSON_Delete(schedules); return true;
}
J *schedule_target(const char *id) {
    if(strncmp(id,"schedule:",9)) return nullptr;
    auto *sys=get(raw,"sysParams"); if(strcmp(str(get(sys,"controllerID")),"ecoMAX860P-LZ TOUCH")||strcmp(str(get(sys,"moduleASoftVer")),"8.41.38.L0")) return nullptr;
    const char *key=id+9; if(!*key||strlen(key)>40) return nullptr; for(const char *p=key;*p;++p) if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_')) return nullptr;
    bool direct=selected_mode=="rs485";
    auto *a=get(get(direct?body("rsSchedules"):get(sys,"schedules"),"ecomaxSchedules"),key); if(!valid_schedule(a)) return nullptr;
    int index=rs_schedule_index(key);
    uint64_t schedule_ms=index==16?native_room_schedule_ms.load():native_schedule_ms.load();
    if(direct&&(index<0||!schedule_ms||now()-schedule_ms>=65000||!cJSON_IsNumber(get(get(data,"schedule_write_indices"),key))))return nullptr;
    auto *v=cJSON_CreateObject(); text(v,"id",id); text(v,"name",std::string("Harmonogram ")+key); text(v,"wire_key",key); copy(v,"before",a);
    if(direct) { number(v,"wire_index",index); number(v,"wire_recipient",index==16?81:69);
        if(index==16)number(v,"wire_parameter",num(get(body("rsThermostatSchedule"),"parameter_raw"))); }
    return v;
}
bool schedule_value(const J *t,const J *value) {
    if(!valid_schedule(value)) return false;
    auto *before=item(get(t,"before"),7), *after=item(const_cast<J*>(value),7);
    return equal(num(item(before,2)),num(item(after,2)))&&equal(num(item(before,3)),num(item(after,3)));
}
bool encode_schedule(const J *target,const J *value,uint8_t (&bytes)[46]) {
    int index=rs_schedule_index(str(get(target,"wire_key")));
    if(index<0||num(get(target,"wire_index"))!=index||!schedule_value(target,value))return false;
    bytes[0]=1; bytes[1]=index; bytes[2]=num(item(item(const_cast<J*>(value),7),0));
    bytes[3]=index==16?num(get(target,"wire_parameter")):num(item(item(const_cast<J*>(value),7),1));
    if(index==16) {
        double parameter=num(get(target,"wire_parameter")),normalized=parameter==255?0:parameter;
        if(!((parameter>=0&&parameter<=100&&floor(parameter)==parameter)||parameter==255)||num(item(item(const_cast<J*>(value),7),1))!=normalized||
           num(item(item(const_cast<J*>(value),7),2))!=normalized||num(item(item(const_cast<J*>(value),7),3))!=normalized)return false;
    }
    for(unsigned day=0;day<7;++day)for(unsigned byte=0;byte<6;++byte)bytes[4+day*6+byte]=num(item(item(const_cast<J*>(value),day),byte));
    return true;
}
void build_menu() {
    auto *menu=cJSON_CreateArray(), *categories=cJSON_CreateArray(), *refs=cJSON_CreateObject(); unsigned parent=0;
    J *row; int position=0;
    cJSON_ArrayForEach(row,body("rmStructure")) {
        int kind=static_cast<int>(num(get(row,"type"),-1));
        if (kind==7 && position>2) ++parent;
        else if (kind==0||kind==2||kind==4||kind==5) {
            auto *section=cJSON_CreateObject(); int index=static_cast<int>(num(get(row,"index"),-1));
            std::string name=str(item(body("rmCatsNames"),index),"Grupa"); std::string prefix=str(get(item(categories,parent),"path"));
            number(section,"index",index); text(section,"name",name); text(section,"path",prefix.empty()?name:prefix+" / "+name); copy(section,"locked",get(row,"lock")); copy(section,"pass_index",get(row,"pass_index")); cJSON_AddItemToArray(categories,section);
        } else if (kind==1) { char key[16]; snprintf(key,sizeof(key),"%d",static_cast<int>(num(get(row,"index"),-1))); auto *section=item(categories,parent); put(refs,key,section?cJSON_Duplicate(section,true):cJSON_CreateObject()); }
        ++position;
    }
    int index=0; J *meta;
    cJSON_ArrayForEach(meta,body("rmParamsData")) {
        auto *v=cJSON_CreateObject(); char key[16]; snprintf(key,sizeof(key),"%d",index); auto *ref=get(refs,key);
        number(v,"index",index); text(v,"name",str(item(body("rmParamsNames"),index),"Parametr")); text(v,"description",str(item(body("rmParamsDescs"),index)));
        copy(v,"metadata",meta); copy(v,"unit_name",item(body("rmParamsUnitsNames"),static_cast<int>(num(get(meta,"unit"),-1))));
        copy(v,"category",get(ref,"path")); flag(v,"in_structure",ref!=nullptr); flag(v,"write_enabled",false);
        for(auto &d:definitions) if(!d.legacy&&d.index==index) menu_write(v,d);
        J *access; cJSON_ArrayForEach(access,body("rmStructure")) if(num(get(access,"type"))==1 && num(get(access,"index"))==index) { copy(v,"locked",get(access,"lock")); copy(v,"pass_index",get(access,"pass_index")); break; }
        put(v,"enum_text",enum_value(meta));
        if(selected_mode=="rs485") {
            bool direct_setting=cJSON_IsString(get(v,"write_id"))&&std::isfinite(num(get(meta,"value")));
            text(v,"source",direct_setting?"RS485_CURRENT_PARAMETERS_212":"RS485_REMOTE_MENU_214"); flag(v,"read_available",(direct_setting||(index<168&&native_menu_row_ms[index]&&now()-native_menu_row_ms[index]<=20000))&&std::isfinite(num(get(meta,"value"))));
            text(v,"read_status",cJSON_IsTrue(get(v,"read_available"))?"Odczyt RS485":native_menu_error.empty()?"Parametr nieudostepniony przez sterownik":native_menu_error);
        }
        if(selected_mode=="rs485") for(auto &d:definitions) if(!d.legacy&&d.index==index) { Owned writable{rs_menu_temperature_target(d.id)}; flag(v,"write_enabled",writable.p!=nullptr); }
        cJSON_AddItemToArray(menu,v); ++index;
    }
    const char *ster[][3]={{"STER_MODE","Tryb pracy",""},{"STER_TEMP_DAY","Temperatura zadana dzienna","\u00b0C"},{"STER_TEMP_NIGHT","Temperatura zadana nocna","\u00b0C"},{"STER_TEMP_ANTIFREEZ","Temperatura zadana przeciwzamrozeniowa","\u00b0C"},{"STER_TEMP_SET_PARTY","Temperatura zadana dla trybu Party","\u00b0C"},{"STER_TEMP_SET_SUMMER","Temperatura zadana dla trybu Wakacje","\u00b0C"},{"STER_HYSTEREZE","Histereza temperatury zadanej","\u00b0C"},{"STER_CORRECTION","Korekta wskazan temperatury","\u00b0C"},{"STER_TIMER_EXIT","Czas trwania trybu Wyjscie z domu","h"},{"STER_TIMER_VENT","Czas trwania trybu Wietrzenie","min"},{"STER_TIMER_PARTY","Czas trwania trybu Party","h"},{"STER_TIMER_HOLIDAY","Czas trwania trybu Wakacje","dni"}};
    J *legacy; cJSON_ArrayForEach(legacy,body("editParams")) {
        std::string key=legacy->string?legacy->string:""; auto split=key.rfind('_'); if(split==std::string::npos) continue;
        auto prefix=key.substr(0,split), suffix=key.substr(split+1); if(suffix.size()!=1||suffix[0]<'1'||suffix[0]>'8') continue;
        for(auto &definition:ster) if(prefix==definition[0]) { auto *v=cJSON_CreateObject(); text(v,"index",key); text(v,"name",definition[1]); text(v,"description","Odczyt editParams / ecoSTER T"+suffix); text(v,"category","ecoSTER T"+suffix); text(v,"unit_name",definition[2]); copy(v,"metadata",legacy); flag(v,"in_structure",true); flag(v,"write_enabled",false); text(v,"source","editParams"); put(v,"locked",nullptr); put(v,"pass_index",nullptr); put(v,"enum_text",prefix=="STER_MODE"&&num(get(legacy,"value"))==2?cJSON_CreateString("Tryb komfortowy"):nullptr); cJSON_AddItemToArray(menu,v); }
    }
    cJSON_ArrayForEach(row,menu) {
        auto *d=definition(str(get(row,"index"))); if(!d||!d->legacy) continue;
        if(d->position>=5&&selected_mode!="rs485") continue;
        if(selected_mode=="rs485") {
            Owned t{rs_target(d->id)}; auto *m=get(row,"metadata");
            menu_write(row,*d); flag(row,"write_enabled",t.p!=nullptr); flag(row,"cached",false);
            for(auto pair:{std::pair<const char *,const char *>{"value","before"},{"minv","min"},{"maxv","max"},{"mult","step"}}) copy(m,pair.first,get(t.p,pair.second));
            text(row,"source","RS485_THERMOSTAT_PARAMETERS_220");
            text(row,"value_source",t.p?"RS485 / aktywny profil eSTER":"Brak zgodnego odczytu aktywnego profilu RS485");
            continue;
        }
        auto *curr=get(get(raw,"regParams"),"curr"), *m=get(row,"metadata");
        bool active=room_mode(get(curr,"ecoSterMode1"))==(d->position==3?1:2);
        copy(row,"cached_metadata_value",get(m,"value"));
        // This ecoNET keeps editParams values unchanged after successful thermostat writes.
        number(m,"value",active?round(num(get(curr,d->key))/d->step)*d->step:NAN);
        menu_write(row,*d); flag(row,"write_enabled",selected_mode!="rs485"&&active);
        text(row,"value_source",active?"regParams / aktywny profil eSTER":"Brak aktualnego odczytu nieaktywnego profilu");
    }
    cJSON_ArrayForEach(row,menu) {
        std::string id=cJSON_IsString(get(row,"index"))?"legacy:"+std::string(str(get(row,"index"))):"rm:"+std::to_string(static_cast<int>(num(get(row,"index"))));
        if(cJSON_IsString(get(row,"index"))||!get(row,"write_id")) { Owned v{generic_target(id.c_str())}; if(v.p) { text(row,"write_id",id); flag(row,"write_enabled",true); copy(row,"cached",get(v.p,"cached")); copy(row,"enum_text",nullptr); if(cJSON_GetArraySize(get(v.p,"options"))) { J *o; cJSON_ArrayForEach(o,get(v.p,"options")) if(equal(num(get(o,"value")),num(get(v.p,"before")))) copy(row,"enum_text",get(o,"label")); } copy(get(row,"metadata"),"value",get(v.p,"before")); } }
    }
    // A cold RS485-only boot has no HTTP editParams catalogue; build room rows from the wire.
    if(selected_mode=="rs485") for(auto &d:definitions) if(d.legacy&&d.native) {
        J *entry=nullptr; cJSON_ArrayForEach(row,menu) if(!strcmp(str(get(row,"index")),d.id)) entry=row;
        if(!entry) { entry=cJSON_CreateObject(); cJSON_AddItemToArray(menu,entry); }
        Owned t{rs_target(d.id)}; auto *metadata=cJSON_CreateObject();
        text(entry,"index",d.id); text(entry,"name",d.position==3?"Temperatura zadana nocna":d.position==2?"Temperatura zadana dzienna":d.name);
        text(entry,"description",d.position<=3?"Nastawa aktywnego profilu ecoSTER T1":"Nastawa profilu ecoSTER T1");
        text(entry,"category","ecoSTER T1"); text(entry,"unit_name","\u00b0C");
        flag(entry,"in_structure",true); flag(entry,"locked",false); number(entry,"pass_index",0); flag(entry,"cached",false);
        text(entry,"source","RS485_THERMOSTAT_PARAMETERS_220"); text(entry,"write_id",d.id); flag(entry,"write_enabled",t.p!=nullptr);
        for(auto pair:{std::pair<const char *,const char *>{"value","before"},{"minv","min"},{"maxv","max"}}) copy(metadata,pair.first,get(t.p,pair.second));
        number(metadata,"unit",1); number(metadata,"mult",d.step); flag(metadata,"edit",t.p!=nullptr); put(entry,"metadata",metadata);
        int wire=plum::thermostat_temperature_index(d.position);
        if(!t.p&&wire>=0&&native_room_ms&&now()-native_room_ms<15000&&native_room.values[0][wire].present) {
            const auto &stored=native_room.values[0][wire]; number(metadata,"value",stored.value/10.0); number(metadata,"minv",stored.low/10.0); number(metadata,"maxv",stored.high/10.0);
        }
        text(entry,"value_source",t.p?(cJSON_IsTrue(get(t.p,"active_profile"))?"RS485 / aktywny profil eSTER":"RS485 / pamiec nieaktywnego profilu"):"Brak zgodnego odczytu profilu RS485");
    }
    if(selected_mode=="rs485"&&native_room_ms&&now()-native_room_ms<15000&&native_room_config_ms&&now()-native_room_config_ms<120000) {
        const unsigned wire_indices[]={1,10,11,12,2,3,9,4,5,6,7,8};
        const char *keys[]={"STER_MODE_1","STER_TEMP_DAY_1","STER_TEMP_NIGHT_1","STER_TEMP_ANTIFREEZ_1","STER_TEMP_SET_PARTY_1","STER_TEMP_SET_SUMMER_1","STER_HYSTEREZE_1","STER_CORRECTION_1","STER_TIMER_EXIT_1","STER_TIMER_VENT_1","STER_TIMER_PARTY_1","STER_TIMER_HOLIDAY_1"};
        for(unsigned i=0;i<12;++i) {
            unsigned wire=wire_indices[i]; const auto &value=native_room.values[0][wire-1]; if(!value.present) continue;
            J *entry=nullptr; cJSON_ArrayForEach(row,menu) if(!strcmp(str(get(row,"index")),keys[i])) entry=row;
            auto *config=item(body("rsThermostatConfig"),wire); if(!config) continue;
            double unit=num(get(config,"unit")),mult=num(get(config,"mult")),shift=num(get(config,"offset"));
            if(!std::isfinite(unit)||!std::isfinite(mult)||!std::isfinite(shift)||unit<0||unit>31||(unit!=31&&mult<=0)) continue;
            if(!entry) { entry=cJSON_CreateObject(); text(entry,"index",keys[i]); cJSON_AddItemToArray(menu,entry); flag(entry,"write_enabled",false); }
            auto *metadata=cJSON_CreateObject();
            number(metadata,"value",unit==31?value.value&127:(value.value-shift)*mult); number(metadata,"minv",unit==31?value.low:(value.low-shift)*mult); number(metadata,"maxv",unit==31?value.high:(value.high-shift)*mult);
            number(metadata,"unit",unit); number(metadata,"mult",unit==31?1:mult); number(metadata,"offset",shift); flag(metadata,"edit",cJSON_IsTrue(get(entry,"write_enabled"))); put(entry,"metadata",metadata);
            text(entry,"name",ster[i][1]); text(entry,"category","ecoSTER T1"); text(entry,"source","RS485_THERMOSTAT_PARAMETERS_220");
            text(entry,"unit_name",unit==31?"":wire==8?"dni":str(item(body("rmParamsUnitsNames"),unit)));
            flag(entry,"in_structure",true); flag(entry,"cached",false); flag(entry,"read_available",true);
            if(!definition(keys[i])) {std::string id="legacy:"+std::string(keys[i]); Owned t{rs_room_parameter_target(id.c_str())}; flag(entry,"write_enabled",t.p!=nullptr); text(entry,"write_id",id); flag(metadata,"edit",t.p!=nullptr);}
            if(i==0&&((value.value&127)<=7)) text(entry,"enum_text",room_modes[value.value&127]);
        }
    }
    cJSON_Delete(refs); Lock lock; put(data,"menu",menu); put(data,"categories",categories);
    for(auto pair:{std::pair<const char *,const char *>{"names","rmCurrentDataParams"},{"units","rmParamsUnitsNames"},{"enums","rmParamsEnums"},{"structure","rmStructure"},{"category_names","rmCatsNames"},{"category_descriptions","rmCatsDescs"},{"alarm_names","rmAlarmsNames"},{"legacy_params","editParams"}}) if(body(pair.second)) copy(data,pair.first,body(pair.second));
}
bool rs_refresh(bool background=false);
bool rs_inspect_temperature(const char *id,J *&out);
J *rs_target(const char *id) {
    if(definition(id)&&!definition(id)->native) { fail("Ta nastawa wymaga lokalnego ecoNET"); return nullptr; }
    auto *d=definition(id); if(!d || !transport::enabled() || !native_identity || now()-native_identity_ms>60000) { fail("Brak potwierdzonej tozsamosci RS485"); return nullptr; }
    Owned sensor{boiler::snapshot()};
    if(!cJSON_IsTrue(get(sensor.p,"fresh")) || strcmp(str(get(get(sensor.p,"modules"),"module_a")),"8.41.38.L0")) { fail("Brak swiezego potwierdzenia firmware kotla po RS485"); return nullptr; }
    if(d->legacy) {
        if(!native_room_ms || now()-native_room_ms>15000 || native_room.slots!=3 || num(get(sensor.p,"thermostat_slots"))!=3 ||
           strcmp(str(get(get(sensor.p,"modules"),"ecoster")),"1.21.38")) { fail("Brak zweryfikowanej mapy termostatu RS485"); return nullptr; }
        J *room=nullptr,*row; cJSON_ArrayForEach(row,get(sensor.p,"thermostats")) if(num(get(row,"index"))==0) room=row;
        int index=plum::thermostat_temperature_index(d->position);
        if(index<0) { fail("Nieznana pozycja nastawy termostatu"); return nullptr; }
        const auto &p=native_room.values[0][index], &m=native_room.values[0][0];
        double mode=room_mode(get(room,"state")),base=p.value/10.0,low=p.low/10.0,high=p.high/10.0,effective=num(get(room,"target_temp"));
        int active_index=std::isfinite(mode)?plum::thermostat_active_index(static_cast<unsigned>(mode)):-1;
        bool active=mode==(d->position==3?1:d->position==2?2:d->position);
        if(!room || !m.present || m.low!=0 || m.high!=7 || (m.value&127)!=mode ||
           !p.present || low<d->low || high>d->high || !aligned(base,d) || !aligned(low,d) || !aligned(high,d) ||
           (active_index>=0&&(!native_room.values[0][active_index].present||!equal(native_room.values[0][active_index].value/10.0,effective))) || !std::isfinite(num(get(room,"current_temp")))) { fail("Brak zgodnego odczytu nastawy i aktywnego profilu RS485"); return nullptr; }
        auto *v=cJSON_CreateObject(); text(v,"id",id); text(v,"name",d->name); number(v,"before",base); number(v,"effective",active?effective:NAN);
        number(v,"active_target",effective); number(v,"parameter_sample_ms",native_room_ms);
        number(v,"min",low); number(v,"max",high); number(v,"step",d->step); text(v,"measured_key",d->measured); text(v,"menu_index",id);
        number(v,"profile_mode",mode); number(v,"profile_raw",m.value); flag(v,"active_profile",active); flag(v,"legacy",true);
        number(v,"wire_index",index+1); number(v,"parameter_count",plum::thermostat_parameters); copy(v,"sample_ms",get(sensor.p,"updated_ms"));
        text(v,"transport","rs485"); text(v,"value_source","RS485 THERMOSTAT_PARAMETERS 220 + SENSOR_DATA 53"); return v;
    }
    auto *meta=get(body("rmCurrentDataParamsEdits"),id);
    double base=num(get(meta,"value")), low=num(get(meta,"min")), high=num(get(meta,"max"));
    if(num(get(meta,"type"))!=4 || !std::isfinite(base)||!std::isfinite(low)||!std::isfinite(high)||low<d->low||high>d->high||base<low||base>high||low>high) { fail("Niezweryfikowane limity nastawy RS485"); return nullptr; }
    double effective=num(get(get(sensor.p,"sensors"),!strcmp(id,"1281")?"water_heater_target":"heating_target"));
    if(!strcmp(id,"1287")) {
        J *mixer=nullptr,*row; cJSON_ArrayForEach(row,get(sensor.p,"mixers")) if(num(get(row,"index"))==0) mixer=row;
        if(!mixer||!native_mixer.sample||now()-native_mixer.sample>15000||
           !equal(base,native_mixer.value)||!equal(low,native_mixer.low)||!equal(high,native_mixer.high)) { fail("Brak zgodnej mapy mieszacza 1: 178 i 212"); return nullptr; }
        effective=num(get(mixer,"target_temp"));
        if(!std::isfinite(effective)) { fail("Brak aktualnego celu mieszacza"); return nullptr; }
    }
    auto *v=cJSON_CreateObject(); text(v,"id",id); text(v,"name",d->name); number(v,"before",base); number(v,"effective",effective); number(v,"difference",base-effective); number(v,"menu_index",d->index); number(v,"min",low); number(v,"max",high); number(v,"step",d->step); text(v,"measured_key",d->measured); text(v,"transport","rs485");
    if(!strcmp(id,"1287")) { number(v,"wire_index",0); number(v,"parameter_sample_ms",native_mixer.sample); number(v,"sample_ms",num(get(sensor.p,"updated_ms"))); }
    return v;
}
J *target(const char *id) {
    auto *d=definition(id); if(!d) { fail("Ten parametr jest tylko do odczytu"); return nullptr; }
    auto *sys=get(raw,"sysParams");
    if(strcmp(str(get(sys,"controllerID")),"ecoMAX860P-LZ TOUCH") || strcmp(str(get(sys,"moduleASoftVer")),"8.41.38.L0")) { fail("Niezweryfikowany model lub firmware. Zapis zablokowany"); return nullptr; }
    auto *curr=get(get(raw,"regParams"),"curr");
    if(d->legacy) {
        if(d->position>=5) { fail("Ten identyfikator profilu wymaga RS485; ecoNET korzysta z parametru menu"); return nullptr; }
        auto *m=get(body("editParams"),id), *mode_meta=get(body("editParams"),"STER_MODE_1");
        double base=num(get(m,"value")), low=num(get(m,"minv")), high=num(get(m,"maxv"));
        double mode=room_mode(get(curr,"ecoSterMode1")), effective=num(get(curr,d->key));
        if(!cJSON_IsTrue(get(m,"edit"))||num(get(m,"unit"))!=1||!equal(num(get(m,"mult")),d->step)||num(get(m,"sec"))!=36||num(get(m,"pos"))!=d->position
           ||!aligned(base,d)||!aligned(low,d)||!aligned(high,d)||low<d->low||high>d->high||low>high||base<low||base>high) { fail("Niezweryfikowane metadane temperatury eSTER"); return nullptr; }
        if(!std::isfinite(num(get(curr,d->measured)))||!aligned(effective,d)||effective<low||effective>high||!std::isfinite(mode)||floor(mode)!=mode||mode<0||mode>7
           ||num(get(mode_meta,"sec"))!=36||num(get(mode_meta,"pos"))!=1||num(get(mode_meta,"unit"))!=31) { fail("Brak spojnego odczytu termostatu eSTER"); return nullptr; }
        bool active=mode==(d->position==3?1:2);
        if(!active) { fail("Nastawa nieaktywna: brak aktualnego odczytu profilu eSTER"); return nullptr; }
        auto *v=cJSON_CreateObject(); text(v,"id",id); text(v,"name",d->name); number(v,"before",round(effective/d->step)*d->step); number(v,"effective",effective); number(v,"min",low); number(v,"max",high); number(v,"step",d->step); text(v,"measured_key",d->measured); text(v,"menu_index",id); number(v,"profile_mode",mode); flag(v,"active_profile",active); flag(v,"legacy",true); number(v,"metadata_value",base); flag(v,"metadata_in_sync",equal(base,effective)); text(v,"value_source","regParams / aktywny profil eSTER"); return v;
    }
    auto *meta=get(body("rmCurrentDataParamsEdits"),id), *m=item(body("rmParamsData"),d->index);
    double low=num(get(meta,"min")), high=num(get(meta,"max")), base=num(get(meta,"value"));
    if (num(get(meta,"type"))!=4||!std::isfinite(low)||!std::isfinite(high)||!std::isfinite(base)||low<d->low||high>d->high||low>high||base<low||base>high) { fail("Brak zgodnych metadanych nastawy"); return nullptr; }
    if(strcmp(str(item(body("rmParamsNames"),d->index)),d->name)) { fail("Niezgodna nazwa nastawy w menu"); return nullptr; }
    unsigned matches=0; J *access;
    cJSON_ArrayForEach(access,body("rmStructure")) if(num(get(access,"type"))==1&&num(get(access,"index"))==d->index) { if(!cJSON_IsFalse(get(access,"lock"))||num(get(access,"pass_index"))!=0) { fail("Parametr zablokowany lub serwisowy"); return nullptr; } ++matches; }
    if(matches!=1 || num(get(m,"value"))!=base||num(get(m,"minv"))!=low||num(get(m,"maxv"))!=high||!cJSON_IsTrue(get(m,"edit"))||num(get(m,"unit"))!=1||num(get(m,"mult"),1)!=1||num(get(m,"offset"),0)!=0) { fail("Niespojne nastawy z menu i metadanych"); return nullptr; }
    double effective=num(get(body("regParamsData"),id)); double named=num(get(get(get(raw,"regParams"),"curr"),d->key));
    if(!equal(effective,named)||!aligned(base,d)) { fail("Niespojne dane celu temperatury"); return nullptr; }
    auto *v=cJSON_CreateObject(); text(v,"id",id); text(v,"name",d->name); number(v,"before",base); number(v,"effective",effective); number(v,"difference",base-effective); number(v,"menu_index",d->index); number(v,"min",low); number(v,"max",high); copy(v,"settings_version",get(get(raw,"regParams"),"settingsVer"));
    number(v,"step",d->step); text(v,"measured_key",d->measured);
    auto *s=item(get(get(get(get(raw,"sysParams"),"schedules"),"ecomaxSchedules"),d->schedule),7);
    put(v,"schedule_enabled",s?cJSON_CreateBool(num(item(s,0))==1):nullptr); copy(v,"schedule_reduction",item(s,1)); return v;
}
void publish(bool editable=true) {
    auto *curr=get(get(raw,"regParams"),"curr"), *values=body("regParamsData");
    if (!cJSON_IsObject(curr)||!cJSON_IsObject(values)) { fail("Brak biezacych danych regulatora"); return; }
    auto *targets=cJSON_CreateObject(), *errors=cJSON_CreateObject();
    for(auto &d:definitions) { auto *v=editable?target(d.id):nullptr; if(v) put(targets,d.id,v); else text(errors,d.id,error_text); }
    if(editable&&selected_mode!="rs485") {
        J *row; cJSON_ArrayForEach(row,get(data,"menu")) { const char *id=str(get(row,"write_id")); if(generic_id(id)) { auto *v=generic_target(id); if(v) put(targets,id,v); } }
    }
    Lock lock; copy(data,"curr",curr); copy(data,"values",values); copy(data,"edits",body("rmCurrentDataParamsEdits"));
    put(data,"targets",targets); put(data,"target_errors",errors);
    auto *versions=cJSON_Duplicate(get(raw,"regParams"),true); cJSON_DeleteItemFromObjectCaseSensitive(versions,"curr"); put(data,"versions",versions);
    live_ms=now(); target_ms=editable?now():0; number(data,"last_read_epoch",epoch()); text(data,"last_read_utc",timestamp()); number(data,"target_read_epoch",editable?epoch():0); flag(data,"connected",true); if(editable) put(data,"error",nullptr); else text(data,"error","Dane biezace aktualne; ponawianie odczytu nastaw ecoNET");
    panel_history::append(curr,false,epoch(),now());
}
bool refresh(bool system=false) {
    if(selected_mode=="rs485") return rs_refresh();
    if (system || !system_ms || now()-system_ms>30000) if(!identify()) return false;
    for(auto endpoint:{"regParams","regParamsData"}) if(!read(endpoint)) { Lock lock; target_ms=0; put(data,"targets",cJSON_CreateObject()); text(data,"error",error_text); return false; }
    for(auto endpoint:{"rmCurrentDataParamsEdits","rmParamsData","editParams"}) if(!read(endpoint)) { publish(false); build_menu(); return false; }
    build_menu(); publish(); return true;
}
bool scan() {
    if(selected_mode=="rs485") return rs_refresh();
    if(!identify()) return false;
    const char *list[]={"regParams","regParamsData","rmCurrentDataParams","rmCurrentDataParamsEdits","rmParamsData","rmParamsNames","rmParamsDescs","rmParamsUnitsNames","rmParamsEnums","rmStructure","rmAlarmsNames","rmCatsNames","rmCatsDescs","editParams"};
    int i=0; bool required=true;
    for(auto e:list) {
        { Lock lock; text(get(data,"job"),"step",e); }
        bool ok=read(e,strstr(e,"Names")||strstr(e,"Descs")||!strcmp(e,"rmParamsEnums")||!strcmp(e,"rmStructure"));
        if(!ok&&(i<6||!strcmp(e,"editParams")||!strcmp(e,"rmStructure"))) required=false;
        ++i; vTaskDelay(pdMS_TO_TICKS(70));
    }
    if(!required) return fail("Nie pobrano wymaganego katalogu ecoNET. Sprobuj ponowic odczyt");
    build_menu(); publish(); return true;
}
bool inspect(const char *id, J *&out) {
    if(!definition(id)) return fail("Ten parametr jest tylko do odczytu");
    if(selected_mode=="rs485"&&!definition(id)->native) return fail("Ta nastawa wymaga lokalnego ecoNET");
    if(selected_mode=="rs485") return rs_inspect_temperature(id,out);
    if(!identify()||!read("rmParamsNames",true)||!read("rmStructure",true)||!refresh()) return false;
    out=target(id); return out!=nullptr;
}
bool rs_identify() {
    transport::Reply reply;
    if(!transport::request(57,nullptr,0,185,reply) || reply.length<20) return fail("Brak identyfikacji kotla po RS485");
    auto *p=reply.bytes+8; size_t size=reply.length-10, at=3;
    if(p[0]!=0 || at>=size) return fail("Niezweryfikowany typ kotla RS485");
    size_t length=p[at++]; if(length==0||length>16||at+length+5>size) return fail("Niepoprawny UID RS485");
    uint8_t bytes[18]{}; memcpy(bytes,p+at,length); at+=length;
    uint16_t crc=0xa3a3; for(size_t i=0;i<length;++i) { crc^=bytes[i]; for(unsigned bit=0;bit<8;++bit) crc=(crc&1)?(crc>>1)^0xa001:crc>>1; }
    bytes[length]=crc; bytes[length+1]=crc>>8;
    std::string identity; const char alphabet[]="0123456789ABCDEFGHIJKLMNZPQRSTUV";
    for(int group=((length+2)*8+4)/5-1;group>=0;--group) {
        unsigned v=0; for(unsigned bit=0;bit<5;++bit) { unsigned pos=group*5+bit; if(pos<(length+2)*8) v|=((bytes[pos/8]>>(pos%8))&1)<<bit; }
        if(v||!identity.empty()) identity+=alphabet[v];
    }
    at+=4; size_t model_length=p[at++];
    bool supported=at+model_length==size&&model_length==strlen("ecoMAX860P-LZ TOUCH")&&!memcmp(p+at,"ecoMAX860P-LZ TOUCH",model_length);
    if(!remember_identity(identity.c_str(),supported)) { native_identity=false; return false; }
    text(get(raw,"sysParams"),"uid",uid);
    native_identity=true; native_identity_ms=now(); return true;
}
bool rs_menu_catalog(bool check_versions=false) {
    const unsigned expected=cJSON_GetArraySize(body("rmParamsData"));
    Owned sensor{boiler::snapshot()};
    if(expected!=168||!native_identity||!transport::enabled()||!cJSON_IsTrue(get(sensor.p,"fresh"))||strcmp(str(get(get(sensor.p,"modules"),"module_a")),"8.41.38.L0")||strcmp(str(get(get(sensor.p,"modules"),"panel")),"8.30.28")) { native_menu_ms=0; std::fill_n(native_menu_row_ms,168,0); native_menu_error="Brak zweryfikowanego katalogu RS485"; return false; }
    if(check_versions||!native_menu_versions_ms||now()-native_menu_versions_ms>=60000||!native_structure_ms||now()-native_structure_ms>=60000) {
        const uint8_t query[]={0}; transport::Reply reply;
        if(!transport::request_readonly(86,query,sizeof(query),214,reply)) { native_menu_error="Brak wersji katalogu menu RS485"; return false; }
        Owned parsed{protocol::describe(reply.bytes,reply.length)};
        if(strcmp(str(get(parsed.p,"state")),"decoded")||!menu_versions_match(get(parsed.p,"data"))) { native_menu_ms=native_menu_versions_ms=0; std::fill_n(native_menu_row_ms,168,0); native_menu_error="Wersje nazw, opcji lub uprawnien menu zmienily sie"; return false; }
        int version=structure_version(get(parsed.p,"data"));
        if(version!=native_structure_version||!native_structure_ms||now()-native_structure_ms>=60000) {
            const uint8_t structure_query[]={9}; transport::Reply structure_reply,verify_reply;
            if(!transport::request_readonly(86,structure_query,1,214,structure_reply)) { native_menu_error="Brak aktualnej struktury menu RS485"; return false; }
            Owned actual{protocol::describe(structure_reply.bytes,structure_reply.length)}; auto *structure=get(get(actual.p,"data"),"structure");
            if(strcmp(str(get(actual.p,"state")),"decoded")||!menu_structure_valid(structure)) { native_structure_ms=native_menu_ms=0; std::fill_n(native_menu_row_ms,168,0); native_menu_error="Niezweryfikowany uklad lub uprawnienia menu RS485"; return false; }
            if(!transport::request_readonly(86,query,1,214,verify_reply)) { native_menu_error="Nie potwierdzono wersji nowej struktury menu"; return false; }
            Owned verified{protocol::describe(verify_reply.bytes,verify_reply.length)};
            if(strcmp(str(get(verified.p,"state")),"decoded")||!menu_versions_match(get(verified.p,"data"))||structure_version(get(verified.p,"data"))!=version) { native_structure_ms=0; native_menu_error="Struktura menu zmienila sie podczas odczytu"; return false; }
            auto *wrapper=cJSON_CreateObject(); copy(wrapper,"data",structure); put(raw,"rmStructure",wrapper);
            if(version!=native_structure_version) { native_menu_ms=0; std::fill_n(native_menu_row_ms,168,0); }
            native_structure_version=version; native_structure_ms=now();
        }
        native_menu_versions_ms=now();
    }
    if(!native_menu_config_ms||now()-native_menu_config_ms>=60000) {
        const uint8_t query[]={3}; transport::Reply reply;
        if(!transport::request_readonly(86,query,sizeof(query),214,reply)) { native_menu_error="Brak odpowiedzi metadanych menu RS485"; return false; }
        Owned parsed{protocol::describe(reply.bytes,reply.length)}; auto *details=get(parsed.p,"data"), *metadata=get(details,"parameters");
        if(strcmp(str(get(parsed.p,"state")),"decoded")||num(get(details,"command"))!=3||cJSON_GetArraySize(metadata)!=expected) { native_menu_ms=0; std::fill_n(native_menu_row_ms,168,0); native_menu_error="Niezgodny format metadanych menu RS485"; return false; }
        for(unsigned i=0;i<expected;++i) {
            auto *reference=item(body("rmParamsData"),i), *actual=item(metadata,i);
            for(auto key:{"unit","mult","offset"}) if(!equal(num(get(reference,key)),num(get(actual,key)))) { native_menu_ms=0; std::fill_n(native_menu_row_ms,168,0); native_menu_error="Metadane menu niezgodne z katalogiem tego sterownika"; return false; }
        }
        native_menu_config_ms=now();
    }
    return true;
}
bool rs_menu_refresh() {
    if(!rs_menu_catalog()) return false;
    const unsigned expected=cJSON_GetArraySize(body("rmParamsData"));
    const uint8_t query[]={4,255,0}; transport::Reply reply;
    if(!transport::request_readonly(86,query,sizeof(query),214,reply)) { native_menu_error="Brak odpowiedzi wartosci menu RS485"; return false; }
    Owned parsed{protocol::describe(reply.bytes,reply.length)}; auto *details=get(parsed.p,"data"), *values=get(details,"parameters");
    if(strcmp(str(get(parsed.p,"state")),"decoded")||num(get(details,"command"))!=4||num(get(details,"start"))!=0||cJSON_GetArraySize(values)!=expected) { native_menu_ms=0; std::fill_n(native_menu_row_ms,168,0); native_menu_error="Niepelny odczyt menu RS485"; return false; }
    for(unsigned i=0;i<expected;++i) {
        auto *row=item(values,i), *metadata=item(body("rmParamsData"),i);
        double unit=num(get(metadata,"unit")),mult=num(get(metadata,"mult")),shift=num(get(metadata,"offset"));
        if(!cJSON_IsTrue(get(row,"present"))) { put(metadata,"value",nullptr); native_menu_row_ms[i]=0; continue; }
        double value=num(get(row,"value")),low=num(get(row,"min")),high=num(get(row,"max"));
        if(low>high||value<low||value>high) { put(metadata,"value",nullptr); native_menu_row_ms[i]=0; continue; }
        number(metadata,"value",unit==31?value:(value-shift)*mult); number(metadata,"minv",unit==31?low:(low-shift)*mult); number(metadata,"maxv",unit==31?high:(high-shift)*mult);
        native_menu_row_ms[i]=now();
    }
    native_menu_ms=now(); native_menu_error.clear(); return true;
}
bool rs_menu_point(unsigned index) {
    if(index>=168) return fail("Nieprawidlowy indeks nastawy RS485");
    const uint8_t query[]={4,1,static_cast<uint8_t>(index)}; transport::Reply reply;
    if(!transport::request_readonly(86,query,sizeof(query),214,reply)) return fail("Brak odpowiedzi wybranej nastawy RS485. Nie ponowiono zapisu");
    Owned parsed{protocol::describe(reply.bytes,reply.length)}; auto *details=get(parsed.p,"data"), *values=get(details,"parameters"), *row=item(values,0);
    if(strcmp(str(get(parsed.p,"state")),"decoded")||num(get(details,"command"))!=4||num(get(details,"start"))!=index||cJSON_GetArraySize(values)!=1||num(get(row,"index"))!=index||!cJSON_IsTrue(get(row,"present"))) return fail("Niezgodna odpowiedz wybranej nastawy RS485");
    double value=num(get(row,"value")),low=num(get(row,"min")),high=num(get(row,"max"));
    if(!std::isfinite(value)||!std::isfinite(low)||!std::isfinite(high)||low>high||value<low||value>high) return fail("Nieprawidlowa wartosc lub zakres nastawy RS485");
    auto *meta=item(body("rmParamsData"),index); double unit=num(get(meta,"unit")),mult=num(get(meta,"mult")),offset=num(get(meta,"offset"));
    number(meta,"value",unit==31?value:(value-offset)*mult); number(meta,"minv",unit==31?low:(low-offset)*mult); number(meta,"maxv",unit==31?high:(high-offset)*mult);
    // A point read must not renew the age of any other parameter.
    native_menu_row_ms[index]=now(); return true;
}
J *rs_menu_temperature_target(const char *id) {
    auto *d=definition(id); if(!d||d->legacy) return nullptr;
    std::string key="rm:"+std::to_string(d->index); Owned point{generic_target(key.c_str())}; if(!point.p) return nullptr;
    if(strcmp(str(get(point.p,"name")),d->name)||num(get(point.p,"wire_unit"))!=1||num(get(point.p,"wire_mult"))!=1||num(get(point.p,"wire_offset"))!=0||num(get(point.p,"min"))<d->low||num(get(point.p,"max"))>d->high||num(get(point.p,"step"))!=d->step) return nullptr;
    Owned sensor{boiler::snapshot()}; if(!cJSON_IsTrue(get(sensor.p,"fresh"))) return nullptr;
    double effective=num(get(get(sensor.p,"sensors"),!strcmp(id,"1281")?"water_heater_target":"heating_target"));
    if(d->index==62) {
        J *mixer=nullptr,*row; cJSON_ArrayForEach(row,get(sensor.p,"mixers")) if(num(get(row,"index"))==0) mixer=row;
        if(!mixer||!native_mixer.sample||now()-native_mixer.sample>15000||!equal(native_mixer.low,num(get(point.p,"min")))||!equal(native_mixer.high,num(get(point.p,"max")))) return nullptr;
        effective=num(get(mixer,"target_temp"));
    }
    auto *target=cJSON_CreateObject(); for(auto key:{"name","before","min","max","step","menu_index"}) copy(target,key,get(point.p,key));
    text(target,"id",id); text(target,"transport","rs485"); text(target,"measured_key",d->measured); number(target,"effective",effective); number(target,"difference",num(get(target,"before"))-effective);
    number(target,"parameter_sample_ms",native_menu_row_ms[d->index]); flag(target,"write_ready",true); return target;
}
void publish_selected(const char *id,const J *target) {
    auto *d=definition(id); int index=d&&!d->legacy?d->index:static_cast<int>(num(get(target,"menu_index"),-1));
    Lock lock; copy(get(data,"targets"),id,target);
    if(index>=0&&index<168) {
        std::string alias="rm:"+std::to_string(index); Owned generic{generic_target(alias.c_str())}; if(generic.p) copy(get(data,"targets"),alias.c_str(),generic.p);
        for(auto &entry:definitions) if(!entry.legacy&&entry.index==index) { Owned native{rs_menu_temperature_target(entry.id)}; if(native.p) copy(get(data,"targets"),entry.id,native.p); }
    }
    const char *row_id=!strncmp(id,"legacy:",7)?id+7:id;
    J *row; cJSON_ArrayForEach(row,get(data,"menu")) if(index>=0?num(get(row,"index"),-1)==index:!strcmp(str(get(row,"index")),row_id)) {
        auto *meta=get(row,"metadata"); for(auto pair:{std::pair<const char *,const char *>{"value","before"},{"minv","min"},{"maxv","max"}}) copy(meta,pair.first,get(target,pair.second));
        flag(row,"read_available",true); text(row,"read_status","Odczyt wybranej nastawy RS485"); number(row,"read_epoch",epoch());
        if(index>=0) { put(row,"enum_text",enum_value(meta)); text(row,"source","RS485_SINGLE_MENU_214"); }
    }
}
bool rs_room_refresh() {
    const uint8_t query[]={255,0}; transport::Reply reply;
    if(!transport::request_readonly(92,query,2,220,reply)||!plum::decode_thermostats(reply.bytes+8,reply.length-10,native_room)) { native_room_ms=0; return fail("Brak odczytu nastawy termostatu RS485"); }
    native_room_ms=now(); return true;
}
bool rs_inspect_temperature(const char *id,J *&out) {
    auto *d=definition(id); if(!d) return false;
    if((!native_identity||now()-native_identity_ms>30000)&&!rs_identify()) return false;
    if(d->legacy) { if(!rs_room_refresh()) return false; out=rs_target(id); }
    else { if(!rs_menu_catalog(true))return fail(native_menu_error.empty()?"Brak potwierdzenia katalogu RS485":native_menu_error.c_str()); if(!rs_menu_point(d->index)) return false; out=rs_menu_temperature_target(id); }
    if(!out) return fail("Brak zgodnego odczytu nastawy RS485");
    publish_selected(id,out); return true;
}
J *rs_temperature_readback(const char *id,const J *baseline) {
    auto *d=definition(id); if(!d) return nullptr;
    if(!d->legacy) { if(!rs_menu_point(d->index)) return nullptr; return rs_menu_temperature_target(id); }
    if(!rs_room_refresh()||native_room.slots!=3) return nullptr;
    Owned sensor{boiler::snapshot()};
    if(!cJSON_IsTrue(get(sensor.p,"fresh"))||strcmp(str(get(get(sensor.p,"modules"),"ecoster")),"1.21.38")) return nullptr;
    int index=plum::thermostat_temperature_index(d->position); if(index<0) return nullptr;
    const auto &value=native_room.values[0][index], &mode=native_room.values[0][0];
    int active=plum::thermostat_active_index(mode.value&127);
    if(!mode.present||!value.present||(active>=0&&!native_room.values[0][active].present)||!equal(mode.value,num(get(baseline,"profile_raw")))||!equal(value.low/10.0,num(get(baseline,"min")))||!equal(value.high/10.0,num(get(baseline,"max")))) return nullptr;
    bool selected=cJSON_IsTrue(get(baseline,"active_profile"));
    double active_target=active>=0?native_room.values[0][active].value/10.0:NAN;
    if(active>=0&&!selected&&!equal(active_target,num(get(baseline,"active_target")))) return nullptr;
    auto *target=cJSON_CreateObject();
    for(auto key:{"id","name","min","max","step","wire_index","parameter_count","legacy","active_profile","profile_mode","profile_raw","measured_key","transport"}) copy(target,key,get(baseline,key));
    number(target,"before",value.value/10.0); number(target,"active_target",active_target); number(target,"parameter_sample_ms",native_room_ms);
    J *room=nullptr,*row; cJSON_ArrayForEach(row,get(sensor.p,"thermostats")) if(num(get(row,"index"))==0) room=row;
    number(target,"effective",selected?num(get(room,"target_temp")):NAN); return target;
}
bool rs_refresh(bool background) {
    auto pending=[&] { return background&&busy.load(); };
    if(pending())return true;
    if(!transport::enabled()) return fail(transport::conflict()?"Konflikt adresu ecoNET. Nadawanie RS485 wylaczone":"RS485 oczekuje na wolny adres przez 30 sekund");
    if(!native_identity || now()-native_identity_ms>30000) if(!rs_identify()) return false;
    if(pending())return true;
    if(now()>=next_menu_poll) { rs_menu_refresh(); next_menu_poll=now()+6000; }
    if(pending())return true;
    transport::Reply reply;
    if(!transport::request(84,nullptr,0,212,reply)) { Lock lock; target_ms=0; return fail("Brak odczytu nastaw RS485"); }
    Owned parsed{protocol::describe(reply.bytes,reply.length)};
    if(strcmp(str(get(parsed.p,"state")),"decoded")) return fail("Niepoprawna odpowiedz nastaw RS485");
    auto *edits=cJSON_CreateObject(); J *row;
    cJSON_ArrayForEach(row,get(get(parsed.p,"data"),"parameters")) {
        char key[12]; snprintf(key,sizeof(key),"%.0f",num(get(row,"id")));
        auto *v=cJSON_CreateObject(); copy(v,"value",get(row,"value")); copy(v,"min",get(row,"min")); copy(v,"max",get(row,"max")); copy(v,"type",get(row,"wire_type")); put(edits,key,v);
    }
    auto *wrap=cJSON_CreateObject(); put(wrap,"data",edits); put(raw,"rmCurrentDataParamsEdits",wrap);
    for(auto &d:definitions) if(d.native&&!d.legacy) { auto *meta=get(edits,d.id), *m=item(body("rmParamsData"),d.index); copy(m,"value",get(meta,"value")); copy(m,"minv",get(meta,"min")); copy(m,"maxv",get(meta,"max")); }
    if(pending())return true;
    native_room_ms=0;
    const uint8_t room_request[]={255,0}; transport::Reply room_reply;
    if(transport::request_readonly(92,room_request,sizeof(room_request),220,room_reply) && room_reply.length>=10 &&
       plum::decode_thermostats(room_reply.bytes+8,room_reply.length-10,native_room)) native_room_ms=now();
    if(pending())return true;
    if(native_room_ms&&(!native_room_config_ms||now()-native_room_config_ms>=60000)) {
        transport::Reply config_reply;
        if(transport::request_readonly(94,room_request,sizeof(room_request),222,config_reply)) {
            Owned metadata{protocol::describe(config_reply.bytes,config_reply.length)}; auto *details=get(metadata.p,"data");
            if(!strcmp(str(get(metadata.p,"state")),"decoded")&&num(get(details,"start"))==0&&num(get(details,"count"))==37) {
                auto *wrap=cJSON_CreateObject(); copy(wrap,"data",get(details,"parameters")); put(raw,"rsThermostatConfig",wrap); native_room_config_ms=now();
            }
        }
    }
    if(pending())return true;
    native_mixer={}; const uint8_t mixer_request[]={16,0}; transport::Reply mixer_reply;
    if(transport::request_readonly(50,mixer_request,sizeof(mixer_request),178,mixer_reply)&&mixer_reply.length==182&&
       mixer_reply.bytes[8]==0&&mixer_reply.bytes[9]==0&&mixer_reply.bytes[10]==14&&mixer_reply.bytes[11]==4) {
        native_mixer.value=mixer_reply.bytes[12]; native_mixer.low=mixer_reply.bytes[13]; native_mixer.high=mixer_reply.bytes[14]; native_mixer.sample=now();
    }
    auto *targets=cJSON_CreateObject(), *errors=cJSON_CreateObject();
    for(auto &d:definitions) {
        auto *v=rs_target(d.id);
        if(v) {
            Owned writable{d.legacy?nullptr:rs_menu_temperature_target(d.id)}; bool ready=d.legacy||writable.p;
            flag(v,"write_ready",ready); if(!ready)text(v,"write_error",native_menu_error.empty()?"Brak zweryfikowanego odczytu pozycji menu":native_menu_error);
            put(targets,d.id,v);
        } else text(errors,d.id,error_text);
    }
    build_menu();
    for(unsigned index=0;index<cJSON_GetArraySize(body("rmParamsData"));++index) {
        std::string id="rm:"+std::to_string(index); auto *target=generic_target(id.c_str()); if(target) put(targets,id.c_str(),target);
    }
    for(auto key:room_wire_keys) {std::string id="legacy:"+std::string(key); if(auto *t=rs_room_parameter_target(id.c_str()))put(targets,id.c_str(),t);}
    { Lock lock; put(data,"targets",targets); put(data,"target_errors",errors); copy(data,"edits",edits); target_ms=now(); number(data,"target_read_epoch",epoch()); put(data,"error",nullptr);
      number(data,"menu_read_epoch",native_menu_ms?epoch()-(now()-native_menu_ms)/1000.0:NAN); text(data,"menu_read_error",native_menu_error); number(data,"menu_structure_version",native_structure_version); }
    return true;
}
J *rs_probe(const J *payload) {
    const char *family=str(get(payload,"family"));
    if(selected_mode!="rs485"||!transport::enabled()||transport::conflict()) { fail("Diagnostyka wymaga aktywnego RS485 bez konfliktu"); return nullptr; }
    if((!native_identity||now()-native_identity_ms>30000)&&!rs_identify()) return nullptr;
    auto *result=cJSON_CreateObject(), *records=cJSON_CreateArray();
    text(result,"state","odczytane"); text(result,"family",family); flag(result,"readonly",true);
    text(result,"message","Odczyt diagnostyczny RS485; bez zapisu nastaw"); put(result,"records",records);
    unsigned received=0;
    auto read=[&](uint8_t type,uint8_t response,const uint8_t *bytes=nullptr,size_t size=0) {
        if(!transport::enabled()||transport::conflict()) return;
        { Lock lock; text(get(data,"job"),"step","Odczyt RS485 "+std::to_string(type)); }
        transport::Reply reply; bool ok=transport::request_readonly(type,bytes,size,response,reply);
        auto *record=cJSON_CreateObject(); number(record,"request_type",type); number(record,"response_type",response);
        flag(record,"sent",reply.sent); flag(record,"received",ok); flag(record,"retried",false); number(record,"latency_ms",reply.latency_ms);
        std::string request_hex,hex; const char digits[]="0123456789abcdef";
        for(size_t i=0;i<size;++i) { request_hex+=digits[bytes[i]>>4]; request_hex+=digits[bytes[i]&15]; }
        text(record,"request_payload_hex",request_hex);
        if(ok) {
            ++received;
            for(size_t i=0;i<reply.length;++i) { hex+=digits[reply.bytes[i]>>4]; hex+=digits[reply.bytes[i]&15]; }
            text(record,"hex",hex); put(record,"decoded",protocol::describe(reply.bytes,reply.length));
        } else text(record,"error",reply.sent?"Brak odpowiedzi w limicie czasu; nie ponowiono":"Nie wyslano");
        cJSON_AddItemToArray(records,record);
    };
    const uint8_t mixer[]={16,0},boiler[]={32,0},thermostat[]={255,0},alerts[]={0,10};
    if(!strcmp(family,"mixer")) { read(50,178,mixer,2); read(81,209,mixer,2); read(83,211,mixer,2); }
    else if(!strcmp(family,"thermostat")) { read(92,220,thermostat,2); read(94,222,thermostat,2); read(95,223,thermostat,2); }
    else if(!strcmp(family,"boiler")) {
        read(84,212);
        for(unsigned start:{0,32,64,96}) { uint8_t parameters[]={32,static_cast<uint8_t>(start)}; read(49,177,parameters,2); }
        read(80,208,boiler,2); read(82,210,boiler,2);
    } else if(!strcmp(family,"menu")) { const uint8_t version[]={0},configuration[]={3},values[]={4,255,0}; read(86,214,version,1); read(86,214,configuration,1); read(86,214,values,3); }
    else if(!strcmp(family,"menu_point")) { const uint8_t query[]={4,1,47}; read(86,214,query,3); }
    else if(!strcmp(family,"menu_structure")) { const uint8_t query[]={9}; read(86,214,query,1); }
    else if(!strcmp(family,"schedules")) read(54,182);
        else if(!strcmp(family,"thermostat_schedules")) {
            transport::Reply reply; bool ok=transport::read_thermostat_schedule(reply);
            auto *record=cJSON_CreateObject();number(record,"request_type",88);number(record,"recipient",81);number(record,"response_type",216);flag(record,"sent",reply.sent);flag(record,"received",ok);
            if(ok){++received;put(record,"decoded",protocol::describe(reply.bytes,reply.length));}
            cJSON_AddItemToArray(records,record);
        }
    else if(!strcmp(family,"alarms")) read(61,189,alerts,2);
    number(result,"responses_received",received); flag(result,"interrupted",!transport::enabled()||transport::conflict());
    state::log("Read-only RS485 probe: family=%s responses=%u; no setting writes",family,received);
    return result;
}
bool save_mode(const char *mode) {
    nvs_handle_t h; if(nvs_open("eco_panel",NVS_READWRITE,&h)!=ESP_OK) return fail("Nie zapisano wyboru polaczenia");
    auto result=nvs_set_str(h,"transport",mode); if(result==ESP_OK) result=nvs_commit(h); nvs_close(h);
    if(result!=ESP_OK) return fail("Nie zapisano wyboru polaczenia");
    selected_mode=mode; ++revision; cJSON_Delete(ticket); ticket=nullptr; native_identity=false; native_room_ms=0; native_room={}; native_mixer={};
    native_menu_ms=native_menu_config_ms=native_menu_versions_ms=next_menu_poll=native_room_config_ms=0; native_menu_error.clear();
    native_structure_ms=native_alarm_ms=next_alarm_poll=next_history_sample=0; native_structure_version=last_pending_alerts=-1; health_failures=alarm_failures=0;
    std::fill_n(native_menu_row_ms,168,0);
    // A new route must not inherit live settings or alarms from the previous one.
    J *row; cJSON_ArrayForEach(row,body("rmParamsData")) put(row,"value",nullptr);
    cJSON_ArrayForEach(row,body("editParams")) put(row,"value",nullptr);
    build_menu();
    { Lock lock; target_ms=live_ms=system_ms=native_schedule_ms=next_schedule_poll=0; native_room_schedule_ms=0; next_room_schedule_poll=0; text(data,"write_transport",mode); put(data,"targets",cJSON_CreateObject());
      put(data,"curr",cJSON_CreateObject()); put(data,"values",cJSON_CreateObject()); put(data,"edits",cJSON_CreateObject());
      put(data,"alarms",nullptr); put(data,"schedules",nullptr); put(data,"series",cJSON_CreateArray()); panel_history::clear();
      put(data,"schedule_status",cJSON_CreateObject()); put(data,"schedule_write_indices",cJSON_CreateObject());
      put(data,"thermostat_schedule_status",cJSON_CreateObject());
      put(data,"endpoints",cJSON_CreateObject()); flag(data,"connected",false); put(data,"error",nullptr);
      text(data,"live_source",!strcmp(mode,"rs485")?"RS485 bezposrednio":"Lokalny ecoNET / HTTP"); }
    return true;
}
J *choose_transport(const J *payload) {
    const char *mode=str(get(payload,"transport"));
    if(strcmp(mode,"rs485")&&strcmp(mode,"econet")&&strcmp(mode,"hybrid")) { fail("Wybierz RS485, ecoNET albo tryb mieszany"); return nullptr; }
    if(!strcmp(mode,"rs485") && !cJSON_IsTrue(get(payload,"confirm_econet_disconnected"))) { fail("Potwierdz odlaczenie ecoNET"); return nullptr; }
    if(!strcmp(mode,"rs485") && !transport::enable()) { fail("Adres ecoNET aktywny w ostatnich 30 sekundach"); return nullptr; }
    if(!save_mode(mode)) { transport::disable(); return nullptr; }
    if(strcmp(mode,"rs485")) transport::disable();
    else { transport::Reply reply; transport::request(25,nullptr,0,0,reply); transport::request(85,nullptr,0,213,reply); }
    auto *out=cJSON_CreateObject(); text(out,"state","selected"); text(out,"transport",mode); text(out,"message",!strcmp(mode,"rs485")?"Wybrano bezposredni RS485":!strcmp(mode,"hybrid")?"Odczyt RS485 pasywny i ecoNET; zapis przez ecoNET. Nadajnik ESP wylaczony":"Wybrano lokalny ecoNET. Nadawanie ESP po RS485 wylaczone"); return out;
}
bool journal() {
    nvs_handle_t h; if(nvs_open("eco_panel",NVS_READWRITE,&h)!=ESP_OK) return fail("Nie otwarto dziennika NVS. Zapis anulowany");
    auto *v=cJSON_CreateObject(); { Lock lock; copy(v,"tests",get(data,"tests")); copy(v,"write_count",get(data,"write_count")); }
    auto *history=get(v,"tests"); while(cJSON_GetArraySize(history)>8) cJSON_DeleteItemFromArray(history,8);
    char *s=cJSON_PrintUnformatted(v);
    while(s&&strlen(s)>6000&&cJSON_GetArraySize(history)>1) { cJSON_free(s); cJSON_DeleteItemFromArray(history,cJSON_GetArraySize(history)-1); s=cJSON_PrintUnformatted(v); }
    if(s&&strlen(s)>6000) { cJSON_free(s); s=nullptr; } cJSON_Delete(v);
    auto result=s?nvs_set_blob(h,"journal",s,strlen(s)+1):ESP_ERR_NO_MEM; if(result==ESP_OK) result=nvs_commit(h);
    cJSON_free(s); nvs_close(h); return result==ESP_OK || fail("Nie utrwalono dziennika. Zapis anulowany");
}
void log_result(const char *kind, const char *result, const J *details=nullptr) {
    Lock lock; auto *v=details?cJSON_Duplicate(details,true):cJSON_CreateObject(); text(v,"time",timestamp()); text(v,"kind",kind); text(v,"result",result);
    auto *tests=get(data,"tests"); cJSON_InsertItemInArray(tests,0,v); while(cJSON_GetArraySize(tests)>25) cJSON_DeleteItemFromArray(tests,25);
}
void phase(const char *step, const char *value, bool *ack=nullptr) { Lock lock; auto *j=get(data,"job"); text(j,"step",step); text(j,"phase",value); if(ack) flag(j,"ack_ok",*ack); }
bool inspect_generic(const char *id,J *&v) {
    if(selected_mode=="rs485") {
        if(!strncmp(id,"schedule:",9)) {
            if((!native_identity||now()-native_identity_ms>30000)&&!rs_identify())return false;
            Owned sensor{boiler::snapshot()};
            if(!cJSON_IsTrue(get(sensor.p,"fresh"))||strcmp(str(get(get(sensor.p,"modules"),"module_a")),"8.41.38.L0")||strcmp(str(get(get(sensor.p,"modules"),"panel")),"8.30.28"))return fail("Brak zweryfikowanej wersji kotla RS485");
            if(rs_schedule_index(id+9)==16?!rs_room_schedule_refresh():!rs_schedule_refresh())return false;
            v=schedule_target(id); return v!=nullptr||fail("Harmonogram nieobecny lub bez zweryfikowanego zapisu RS485");
        }
        if(room_wire_index(id)>0) {
            if((!native_identity||now()-native_identity_ms>30000)&&!rs_identify())return false;
            if(!rs_room_refresh())return false;
            v=rs_room_parameter_target(id);
            if(v)publish_selected(id,v);
            return v!=nullptr||fail("Brak zgodnego odczytu trybu ecoSTER");
        }
        if(strncmp(id,"rm:",3)) return fail("Ta nastawa wymaga lokalnego ecoNET");
        char tail; int index=-1; if(sscanf(id+3,"%d%c",&index,&tail)!=1||index<0||index>=168) return fail("Nieprawidlowy indeks nastawy RS485");
        char canonical[24]; snprintf(canonical,sizeof(canonical),"rm:%d",index); if(strcmp(id,canonical)||!menu_access(index)) return fail("Parametr zablokowany lub niekanoniczny indeks RS485");
        if((!native_identity||now()-native_identity_ms>30000)&&!rs_identify()) return false;
        if(!rs_menu_catalog(true))return fail(native_menu_error.empty()?"Brak potwierdzenia katalogu RS485":native_menu_error.c_str());
        if(!rs_menu_point(index)) return false;
        v=generic_target(id); if(v) publish_selected(id,v); return v!=nullptr||fail("Parametr zablokowany lub bez zgodnego katalogu RS485");
    }
    if(!identify()) return false;
    if(!strncmp(id,"schedule:",9)) v=schedule_target(id);
    else { for(auto endpoint:{"rmParamsNames","rmStructure","rmParamsEnums"}) if(!read(endpoint,true)) return false; if(!refresh()) return false; v=generic_target(id); }
    return v!=nullptr||fail("Parametr zablokowany, nieobecny lub bez potwierdzonej mapy zapisu");
}
bool allow_same_value_audit(J *target,const J *payload) {
    auto *audit=get(payload,"audit_same_value"); if(!audit)return true;
    if(!cJSON_IsBool(audit))return fail("Nieprawidlowa flaga audytu");
    if(!cJSON_IsTrue(audit))return true;
    if(selected_mode!="rs485"||cJSON_IsTrue(get(target,"cached"))||!cJSON_IsTrue(get(payload,"confirm_audit_same_value"))||
       !get(payload,"before")||!cJSON_IsNumber(get(payload,"value"))||!equal(num(get(payload,"value")),num(get(target,"before"))))return fail("Audyt wymaga swiezej, identycznej nastawy RS485 i osobnego potwierdzenia");
    flag(target,"audit_same_value",true); return true;
}
J *prepare_generic(const J *payload,const std::string &owner) {
    const char *id=str(get(payload,"id")); J *v=nullptr; if(!inspect_generic(id,v)) return nullptr;
    bool schedule=!strncmp(id,"schedule:",9); auto *requested=get(payload,"value");
    if(schedule?!schedule_value(v,requested):!generic_value(v,num(requested))) { cJSON_Delete(v); fail("Nieprawidlowa wartosc, zakres, krok lub harmonogram"); return nullptr; }
    if(cJSON_IsTrue(get(v,"risk"))&&!cJSON_IsTrue(get(payload,"confirm_installation"))) { cJSON_Delete(v); fail("Potwierdz zmiane nastawy instalacyjnej"); return nullptr; }
    if(get(payload,"before")&&!cJSON_Compare(get(payload,"before"),get(v,"before"),true)) { cJSON_Delete(v); fail("Nastawa zmienila sie poza formularzem. Odczytaj aktualna wartosc"); return nullptr; }
    if(!allow_same_value_audit(v,payload)){cJSON_Delete(v);return nullptr;}
    if(schedule) { double day=num(get(payload,"day"),0); if(day<0||day>6||floor(day)!=day) { cJSON_Delete(v); fail("Nieprawidlowy dzien tygodnia"); return nullptr; } number(v,"day",day); }
    copy(v,"value",requested); text(v,"kind",schedule?"schedule":"parameter"); text(v,"transport",selected_mode); text(v,"token",random_token()); number(v,"expires",epoch()+60);
    cJSON_Delete(ticket); ticket=cJSON_Duplicate(v,true); ticket_owner=owner; ticket_deadline=now()+60000; ticket_revision=revision; return v;
}
J *execute_generic(const J *t,const J *payload,const J *prepared_now=nullptr) {
    if(selected_mode!=str(get(t,"transport"))) { fail("Droga zapisu zmienila sie"); return nullptr; }
    bool direct=selected_mode=="rs485";
    bool schedule=!strcmp(str(get(t,"kind")),"schedule"); const char *id=str(get(t,"id")); auto started=now();
    phase("Sprawdzanie aktualnej nastawy przed wyslaniem","preflight"); J *p=nullptr;
    if(prepared_now)p=cJSON_Duplicate(prepared_now,true); else if(!inspect_generic(id,p))return nullptr;
    Owned current{p};
    for(auto key:{"before","schema","wire_key","wire_index","wire_width","wire_recipient","wire_parameter","value_kind","cached","profile_mode","profile_raw","observed_key","wire_unit","wire_mult","wire_offset"}) {
        auto *a=get(t,key), *b=get(p,key); if((a||b)&&!cJSON_Compare(a,b,true)) { fail("Nastawa, tryb lub metadane zmienily sie. Zapis anulowany"); return nullptr; }
    }
    if(cJSON_IsTrue(get(t,"risk"))&&!cJSON_IsTrue(get(payload,"confirm_installation"))) { fail("Brak potwierdzenia zmiany nastawy instalacyjnej"); return nullptr; }
    if(schedule?!schedule_value(p,get(t,"value")):!generic_value(p,num(get(t,"value")))) { fail("Zmienily sie limity zapisu"); return nullptr; }
    bool audit=cJSON_IsTrue(get(t,"audit_same_value"));
    if(audit&&(!direct||!cJSON_IsTrue(get(payload,"confirm_audit_same_value"))||!equal(num(get(t,"value")),num(get(p,"before"))))) {fail("Audyt identycznej wartosci anulowany");return nullptr;}
    uint8_t encoded=0; bool room_parameter=direct&&room_wire_index(id)>0; uint16_t room_encoded=0;
    uint8_t schedule_command[46]{};
    if(room_parameter) {
        double n=num(get(p,"wire_unit"))==31?num(get(t,"value")):num(get(t,"value"))/num(get(p,"wire_mult"))+num(get(p,"wire_offset"));
        if(!generic_value(p,num(get(t,"value")))||!std::isfinite(n)||n<0||n>(num(get(p,"wire_width"))==2?65535:255)||!equal(n,round(n))) {fail("Nieprawidlowe kodowanie nastawy ecoSTER");return nullptr;}
        room_encoded=std::lround(n); encoded=room_encoded&255;
    } else if(direct&&(schedule?!encode_schedule(p,get(t,"value"),schedule_command):!encode_menu_value(p,num(get(t,"value")),encoded))) { fail("Wartosc nie ma jednoznacznego kodowania RS485"); return nullptr; }
    auto *result=cJSON_CreateObject(); for(auto k:{"id","name","before","transport","cached"}) copy(result,k,get(t,k)); copy(result,"requested",get(t,"value"));
    flag(result,"audit_same_value",audit);
    if(!audit&&!cJSON_IsTrue(get(t,"cached"))&&cJSON_Compare(get(t,"value"),get(p,"before"),true)) { text(result,"state","unchanged"); text(result,"message","Wartosc jest juz ustawiona. Nie wyslano polecenia"); flag(result,"sent",false); log_result("write","bez_zapisu",result); return result; }
    { Lock lock; number(data,"write_count",num(get(data,"write_count"),0)+1); }
    text(result,"state","pending"); text(result,"message","Trwa pojedynczy zapis nastawy"); log_result("write","pending",result);
    if(!journal()) { Lock lock; number(data,"write_count",num(get(data,"write_count"))-1); cJSON_DeleteItemFromArray(get(data,"tests"),0); cJSON_Delete(result); return nullptr; }
    if(direct&&schedule) {
        bool room=num(get(p,"wire_index"))==16;
        phase("Wysylanie pojedynczego harmonogramu RS485","dispatch"); auto sent=now(); transport::Reply reply;
        if(room)transport::request_thermostat(89,schedule_command,sizeof(schedule_command),217,reply);
        else transport::request(55,schedule_command,sizeof(schedule_command),0,reply);
        auto dispatched=now();
        phase("Odczyt zwrotny harmonogramu bez ponawiania zapisu","readback"); bool matched=false; unsigned attempts=0; Owned observed{nullptr};
        for(unsigned i=0;reply.sent&&i<3;++i) {
            if(i)vTaskDelay(pdMS_TO_TICKS(150));
            ++attempts;
            if(room?!rs_room_schedule_refresh():!rs_schedule_refresh())continue;
            Owned readback{schedule_target(id)}; cJSON_Delete(observed.p); observed.p=cJSON_Duplicate(get(readback.p,"before"),true);
            auto sampled=room?native_room_schedule_ms.load():native_schedule_ms.load();
            if(readback.p&&num(get(readback.p,"wire_index"))==num(get(p,"wire_index"))&&sampled>=sent&&cJSON_Compare(observed.p,get(t,"value"),true)) { matched=true; break; }
        }
        const char *status=!reply.sent?"not_sent":matched?"verified":"unverified";
        text(result,"state",status); flag(result,"sent",reply.sent); flag(result,"ack_expected",false); number(result,"matched_reads",matched?1:0); number(result,"readback_attempts",attempts); copy(result,"observed",observed.p);
        text(result,"message",!reply.sent?"Nie wyslano harmonogramu RS485":matched?"Harmonogram potwierdzono nowym odczytem tygodnia z kotla":"Zapis harmonogramu niepotwierdzony. Nie ponowiono polecenia ani nie przywracano planu automatycznie");
        text(result,"verification_source",room?"ecoSTER 81 / RS485 89 + nowy odczyt 88/216 calego tygodnia":"RS485 55 / jeden zapis + nowy odczyt 54/182 calego tygodnia; protokol nie przewiduje ACK");
        if(room) {flag(result,"ack_expected",true); flag(result,"ack_ok",reply.length>0); number(result,"wire_recipient",81);}
        auto *timing=cJSON_CreateObject(); number(timing,"preflight",(sent-started)/1000.0); number(timing,"write_rs485",(dispatched-sent)/1000.0); number(timing,"readback",(now()-dispatched)/1000.0); number(timing,"total",(now()-started)/1000.0); put(result,"timing_seconds",timing);
        { Lock lock; if(!reply.sent)number(data,"write_count",num(get(data,"write_count"))-1); auto *record=item(get(data,"tests"),0); J *row; cJSON_ArrayForEach(row,result)copy(record,row->string,row); text(record,"result",status); }
        if(!journal())text(result,"journal_warning","Nie utrwalono wyniku w NVS");
        return result;
    }
    if(direct) {
        bool room_mode=!strcmp(id,"legacy:STER_MODE_1");
        uint8_t command[]={5,1,static_cast<uint8_t>(num(get(p,"menu_index"))),encoded}; transport::Reply reply;
        phase("Wysylanie pojedynczej nastawy menu RS485","dispatch"); auto sent=now();
        uint8_t mode_command[]={static_cast<uint8_t>(num(get(p,"wire_index"))),static_cast<uint8_t>(encoded|(room_mode?(static_cast<unsigned>(num(get(p,"profile_raw")))&128):0)),static_cast<uint8_t>(room_encoded>>8)};
        bool ack=room_parameter?(transport::request(93,mode_command,1+num(get(p,"wire_width")),221,reply)&&reply.length==10):
            transport::request(86,command,sizeof(command),214,reply)&&reply.length==13&&reply.bytes[8]==5&&reply.bytes[9]==1&&reply.bytes[10]==command[2];
        auto acknowledged=now(); unsigned matched=0; uint64_t last_sample=0; double observed=NAN;
        phase(ack?"Polecenie przyjete. Trwa odczyt zwrotny":"Brak ACK. Odczyt bez ponawiania polecenia","readback",&ack);
        unsigned attempts=0; auto deadline=now()+8000;
        for(unsigned i=0;reply.sent&&i<24&&now()<deadline;++i) {
            if(i) vTaskDelay(pdMS_TO_TICKS(i<3?150:500));
            if(now()>=deadline)break;
            ++attempts;
            if(room_parameter?rs_room_refresh():rs_menu_point(static_cast<unsigned>(num(get(p,"menu_index"))))) {
                Owned readback{room_parameter?rs_room_parameter_target(id,false):generic_target(id)};
                if(readback.p) {
                    observed=num(get(readback.p,"before"));
                    bool compatible=true; for(auto key:{"schema","wire_key","wire_unit","wire_mult","wire_offset","min","max"}) if(!cJSON_Compare(get(p,key),get(readback.p,key),true)) compatible=false;
                    if(room_parameter&&!room_mode&&!cJSON_Compare(get(p,"profile_raw"),get(readback.p,"profile_raw"),true))compatible=false;
                    if(!compatible) { matched=0; break; }
                    auto sample=room_parameter?native_room_ms:native_menu_row_ms[static_cast<unsigned>(num(get(p,"menu_index")))];
                    if(equal(observed,num(get(t,"value")))) { if(sample>sent&&sample>last_sample) { matched=1; last_sample=sample; publish_selected(id,readback.p); } }
                    else matched=0;
                } else matched=0;
            } else matched=0;
            if(matched) break;
        }
        const char *status=!reply.sent?"not_sent":matched?(ack?"verified":"observed_only"):"unverified";
        text(result,"state",status); flag(result,"sent",reply.sent); flag(result,"ack_ok",ack); number(result,"matched_reads",matched); number(result,"observed",observed); number(result,"wire_value",encoded);
        number(result,"readback_attempts",attempts); number(result,"readback_parameter_count",1); text(result,"read_scope","single_parameter");
        std::string ack_hex; const char digits[]="0123456789abcdef";
        for(size_t i=0;i<reply.length;++i) { ack_hex+=digits[reply.bytes[i]>>4]; ack_hex+=digits[reply.bytes[i]&15]; }
        text(result,"ack_hex",ack_hex);
        text(result,"message",!reply.sent?"Nie wyslano polecenia RS485":matched?(ack?"Nastawe potwierdzono ACK i nowym odczytem wybranego parametru":"Odczytano nowa nastawe, ale brak ACK polecenia"):"Zmiana niepotwierdzona. Nie ponowiono polecenia ani nie przywracano nastawy automatycznie");
        text(result,"verification_source","RS485 REMOTE_MENU 86 / komenda 5 + jeden nowy odczyt 214 / komenda 4, count=1");
        if(room_parameter) {text(result,"verification_source","RS485 93 / nastawa ecoSTER T1 + ACK 221 + nowy odczyt 220");number(result,"readback_parameter_count",37);text(result,"read_scope","thermostat_parameters");}
        if(audit)text(result,"message","Audyt: nadano obecna wartosc; wynik ACK i odczytu w szczegolach. Nie testowano zmiany wartosci");
        auto *timing=cJSON_CreateObject(); number(timing,"preflight",(sent-started)/1000.0); number(timing,"write_rs485",(acknowledged-sent)/1000.0); number(timing,"readback",(now()-acknowledged)/1000.0); number(timing,"total",(now()-started)/1000.0); put(result,"timing_seconds",timing);
        { Lock lock; if(!reply.sent) number(data,"write_count",num(get(data,"write_count"))-1); auto *record=item(get(data,"tests"),0); J *v; cJSON_ArrayForEach(v,result) copy(record,v->string,v); text(record,"result",status); }
        if(!journal()) text(result,"journal_warning","Nie utrwalono wyniku w NVS");
        return result;
    }
    std::string query; const char *endpoint=str(get(t,"endpoint"));
    if(schedule) {
        endpoint="setSchedule"; std::string values; for(int row=0;row<8;++row) { J *v; cJSON_ArrayForEach(v,item(get(t,"value"),row)) { if(!values.empty()) values+=','; values+=wire_value(num(v)); } }
        query="scheduleType=ecomaxSchedules&timeOfWeek="+wire_value(num(get(t,"day")))+"&values="+encode(values)+"&scheduleDev="+encode(str(get(t,"wire_key")));
    } else {
        std::string value=!strcmp(str(get(t,"value_kind")),"boolean")?(num(get(t,"value"))==1?"true":"false"):wire_value(num(get(t,"value")));
        query=std::string(!strcmp(endpoint,"newParam")?"newParamName=":"newParamIndex=")+encode(str(get(t,"wire_key")))+"&newParamValue="+encode(value);
    }
    phase("Wysylanie pojedynczego polecenia do lokalnego ecoNET","dispatch"); auto sent=now(); Owned reply{request(endpoint,query,true)};
    bool ack=reply.p&&!strcmp(str(get(reply.p,"result")),"OK"); auto acknowledged=now(); unsigned matched=0; J *observed=nullptr;
    phase(ack?"Polecenie przyjete. Trwa odczyt zwrotny":"Brak ACK. Odczyt bez ponawiania polecenia","readback",&ack);
    auto deadline=now()+32000; bool cached=cJSON_IsTrue(get(t,"cached"));
    for(unsigned i=0;i<16&&now()<deadline;++i) {
        if(i) vTaskDelay(pdMS_TO_TICKS(matched?500:1500));
        bool ok=schedule?identify():refresh();
        cJSON_Delete(observed); observed=nullptr;
        if(ok) {
            Owned readback{schedule?schedule_target(id):generic_target(id)};
            if(readback.p) {
                if(!schedule&&!strncmp(id,"legacy:",7)&&!cached&&strcmp(str(get(t,"observed_key")),str(get(readback.p,"observed_key")))) { matched=0; break; }
                observed=cJSON_Duplicate(get(readback.p,"before"),true);
                bool same=schedule?cJSON_Compare(observed,get(t,"value"),true):equal(num(observed),num(get(t,"value")));
                matched=!cached&&same?matched+1:0;
            } else matched=0;
        } else matched=0;
        if(matched>=2||cached) break;
    }
    const char *status=matched>=2?(ack?"verified":"observed_only"):cached&&ack?"acknowledged":"unverified";
    text(result,"state",status); flag(result,"sent",true); flag(result,"ack_ok",ack); number(result,"matched_reads",matched); put(result,"observed",observed);
    text(result,"message",matched>=2?"Zapis potwierdzony dwoma odczytami zwrotnymi":cached&&ack?"Polecenie przyjete. ecoNET zwraca pamiec nieaktywnej nastawy; brak niezaleznego potwierdzenia":"Zmiana niepotwierdzona. Nie ponowiono polecenia");
    text(result,"verification_source",schedule?"ecoNET sysParams / schedules":cached?"ACK only / cached editParams":!strncmp(id,"legacy:",7)?"ecoNET regParams / aktywny tryb lub cel eSTER":"ecoNET rmParamsData");
    auto *timing=cJSON_CreateObject(); number(timing,"preflight",(sent-started)/1000.0); number(timing,"write_http",(acknowledged-sent)/1000.0); number(timing,"readback",(now()-acknowledged)/1000.0); number(timing,"total",(now()-started)/1000.0); put(result,"timing_seconds",timing);
    { Lock lock; auto *record=item(get(data,"tests"),0); J *v; cJSON_ArrayForEach(v,result) copy(record,v->string,v); text(record,"result",status); }
    if(!journal()) text(result,"journal_warning","Nie utrwalono wyniku w NVS");
    return result;
}
J *prepare(const J *payload, const std::string &owner) {
    const char *id=str(get(payload,"id")); double value=num(get(payload,"value"));
    if(generic_id(id)) return prepare_generic(payload,owner);
    auto *d=definition(id);
    if(!aligned(value,d)) { fail("Nastawa nie odpowiada krokowi tego parametru"); return nullptr; }
    if(value<d->low||value>d->high) { fail("Nastawa poza dozwolonym zakresem"); return nullptr; }
    J *v=nullptr; if(!inspect(id,v)) return nullptr;
    if(get(payload,"before")&&!equal(num(get(payload,"before")),num(get(v,"before")))) { cJSON_Delete(v); fail("Nastawa zmienila sie poza formularzem. Odczytaj aktualna wartosc"); return nullptr; }
    if(value<num(get(v,"min"))||value>num(get(v,"max"))) { cJSON_Delete(v); fail("Nastawa poza dozwolonym zakresem"); return nullptr; }
    if(!allow_same_value_audit(v,payload)){cJSON_Delete(v);return nullptr;}
    number(v,"value",round(value/d->step)*d->step); text(v,"kind","temperature"); text(v,"transport",selected_mode); text(v,"token",random_token()); number(v,"expires",epoch()+60); cJSON_Delete(ticket); ticket=cJSON_Duplicate(v,true); ticket_owner=owner; ticket_deadline=now()+60000; ticket_revision=revision; return v;
}
J *execute(const J *payload, const std::string &owner,const J *prepared_now=nullptr) {
    Owned held{ticket}; ticket=nullptr;
    if(!held.p||strcmp(str(get(payload,"token")),str(get(held.p,"token")))||now()>ticket_deadline||revision!=ticket_revision||owner!=ticket_owner||!cJSON_IsTrue(get(payload,"confirm"))) { fail("Potwierdzenie wygaslo lub jest nieprawidlowe"); return nullptr; }
    if(!strcmp(str(get(held.p,"kind")),"parameter")||!strcmp(str(get(held.p,"kind")),"schedule")) return execute_generic(held.p,payload,prepared_now);
    if(strcmp(str(get(held.p,"kind")),"temperature")) { fail("Nieprawidlowy rodzaj potwierdzenia"); return nullptr; }
    auto *t=held.p; std::string id=str(get(t,"id")); double value=num(get(t,"value")); auto *d=definition(id.c_str()); uint64_t started=now();
    phase("Sprawdzanie aktualnej nastawy przed wyslaniem","preflight"); J *current=nullptr;
    if(prepared_now)current=cJSON_Duplicate(prepared_now,true); else if(!inspect(id.c_str(),current))return nullptr;
    Owned check{current};
    for(auto k:{"before","min","max","step"}) if(!equal(num(get(t,k)),num(get(current,k)))) { fail("Nastawa lub limity zmienily sie. Zapis anulowany"); return nullptr; }
    if(d->legacy&&!equal(num(get(t,"profile_mode")),num(get(current,"profile_mode")))) { fail("Tryb termostatu zmienil sie. Zapis anulowany"); return nullptr; }
    if(d->legacy&&selected_mode=="rs485") for(auto key:{"wire_index","parameter_count","profile_raw"}) if(!equal(num(get(t,key)),num(get(current,key)))) { fail("Mapa lub profil termostatu RS485 zmienily sie. Zapis anulowany"); return nullptr; }
    if(d->legacy&&selected_mode=="rs485"&&!cJSON_IsTrue(get(t,"active_profile"))&&!equal(num(get(t,"active_target")),num(get(current,"active_target")))) { fail("Aktywny cel pokojowy zmienil sie. Zapis anulowany"); return nullptr; }
    bool audit=cJSON_IsTrue(get(t,"audit_same_value"));
    if(audit&&(selected_mode!="rs485"||!cJSON_IsTrue(get(payload,"confirm_audit_same_value"))||!equal(value,num(get(current,"before"))))) {fail("Audyt identycznej wartosci anulowany");return nullptr;}
    auto *result=cJSON_CreateObject(); for(auto k:{"id","name","before","menu_index","transport"}) copy(result,k,get(t,k)); number(result,"requested",value); copy(result,"effective_before",get(current,"effective"));
    flag(result,"audit_same_value",audit);
    if(!audit&&equal(value,num(get(t,"before")))) { text(result,"state","unchanged"); text(result,"message","Wartosc jest juz ustawiona. Nie wyslano polecenia"); flag(result,"sent",false); log_result("write","bez_zapisu",result); return result; }
    { Lock lock; number(data,"write_count",num(get(data,"write_count"),0)+1); }
    text(result,"state","pending"); text(result,"message","Trwa pojedynczy zapis nastawy"); log_result("write","pending",result);
    if(!journal()) { Lock lock; number(data,"write_count",num(get(data,"write_count"))-1); cJSON_DeleteItemFromArray(get(data,"tests"),0); cJSON_Delete(result); return nullptr; }
    bool direct=selected_mode=="rs485", dispatched=true, ack=false;
    text(result,"transport",direct?"rs485":"econet");
    phase(direct?"Wysylanie pojedynczego polecenia RS485":"Wysylanie pojedynczego polecenia do lokalnego ecoNET","dispatch"); auto sent=now();
    if(direct) {
        transport::Reply reply;
        if(d->legacy) {
            unsigned encoded=static_cast<unsigned>(std::lround(value*10));
            uint8_t payload[]={static_cast<uint8_t>(num(get(current,"wire_index"))),static_cast<uint8_t>(encoded),static_cast<uint8_t>(encoded>>8)};
            ack=transport::request(93,payload,sizeof(payload),221,reply)&&reply.length==10;
        } else if(!strcmp(id.c_str(),"1287")) {
            uint8_t payload[]={0,0,static_cast<uint8_t>(value)};
            ack=transport::request(52,payload,sizeof(payload),180,reply)&&reply.length==10;
        } else {
            uint8_t payload[]={1,0,4,static_cast<uint8_t>(atoi(id.c_str())&255),5,static_cast<uint8_t>(value)};
            ack=transport::request(87,payload,sizeof(payload),215,reply)&&reply.length==10;
        }
        dispatched=reply.sent;
    } else { Owned reply{request(d->legacy?"newParam":"rmCurrNewParam",std::string(d->legacy?"newParamName=":"newParamKey=")+encode(id)+"&newParamValue="+wire_value(value),true)}; ack=reply.p&&!strcmp(str(get(reply.p,"result")),"OK"); }
    auto acknowledged=now();
    phase(ack?"Polecenie przyjete. Trwa odczyt zwrotny":"Brak ACK. Sprawdzanie nastawy bez ponawiania polecenia","readback",&ack);
    unsigned matched=0,attempts=0; double observed=NAN, effective=NAN; auto *errors=cJSON_CreateArray();
    unsigned required=direct?1:2; auto deadline=now()+(direct?8000:32000);
    for(unsigned attempt=0;dispatched&&attempt<(direct?24U:16U)&&now()<deadline;++attempt) {
        if(attempt) vTaskDelay(pdMS_TO_TICKS(direct?(attempt<3?150:500):matched?500:std::min(2000U,500U<<std::min(attempt-1,2U))));
        if(now()>=deadline) break;
        ++attempts;
        bool ok=direct||refresh(); Owned v{direct?rs_temperature_readback(id.c_str(),t):ok?target(id.c_str()):nullptr};
        if(v.p) {
            observed=num(get(v.p,"before")); effective=num(get(v.p,"effective"));
            bool same=equal(observed,value);
            if(direct) {
                same=same&&num(get(v.p,"parameter_sample_ms"))>sent;
                matched=same?1:0; if(same) publish_selected(id.c_str(),v.p);
            } else matched=same?matched+1:0;
        } else { matched=0; cJSON_AddItemToArray(errors,cJSON_CreateString(error_text.c_str())); }
        if(matched>=required) break;
    }
    const char *status=!dispatched?"not_sent":matched>=required?(ack?"verified":"observed_only"):"unverified";
    text(result,"state",status); flag(result,"sent",dispatched); flag(result,"ack_ok",ack); number(result,"observed",observed); number(result,"effective",effective); number(result,"matched_reads",matched); put(result,"read_errors",errors);
    number(result,"readback_attempts",attempts); number(result,"required_reads",required);
    if(direct) { text(result,"read_scope",d->legacy?"thermostat_parameters":"single_parameter"); number(result,"readback_parameter_count",d->legacy?37:1); }
    text(result,"message",!dispatched?"Nie wyslano polecenia. Sprawdz polaczenie RS485":matched>=required?(ack?(direct?"Nastawe potwierdzono ACK i nowym odczytem RS485":d->legacy?"Nastawe pokojowa potwierdzono dwoma odczytami aktywnego celu eSTER":"Nastawe potwierdzono dwoma odczytami menu i metadanych ecoNET"):"Odczytano nowa nastawe, ale brak ACK polecenia"):"Zmiana niepotwierdzona. Nie ponowiono polecenia ani nie przywracano nastawy automatycznie");
    text(result,"verification_source",direct?(d->legacy?"RS485 ACK 221 + jeden nowy odczyt THERMOSTAT_PARAMETERS 220, niezmieniony tryb":"RS485 ACK zapisu + jeden nowy odczyt REMOTE_MENU 214 / komenda 4, count=1"):d->legacy?"ecoNET regParams / aktywny profil eSTER + limity editParams":"ecoNET rmParamsData + rmCurrentDataParamsEdits");
    if(audit)text(result,"message","Audyt: nadano obecna wartosc; wynik ACK i odczytu w szczegolach. Nie testowano zmiany wartosci");
    auto *timing=cJSON_CreateObject(); number(timing,"preflight",(sent-started)/1000.0); number(timing,direct?"write_rs485":"write_http",(acknowledged-sent)/1000.0); number(timing,"readback",(now()-acknowledged)/1000.0); number(timing,"total",(now()-started)/1000.0); put(result,"timing_seconds",timing);
    { Lock lock; if(!dispatched) number(data,"write_count",num(get(data,"write_count"))-1); auto *record=item(get(data,"tests"),0); J *v; cJSON_ArrayForEach(v,result) copy(record,v->string,v); text(record,"result",status); }
    if(!journal()) { text(result,"journal_warning","Wynik odczytano, ale nie utrwalono go w NVS"); }
    return result;
}
J *save_setting(const J *payload,const std::string &owner) {
    if(!cJSON_IsTrue(get(payload,"confirm"))) { fail("Brak potwierdzenia zapisu"); return nullptr; }
    Owned args{cJSON_Duplicate(payload,true)};
    // An absolute setpoint uses the current device value, not the browser's old baseline.
    // Schedule edits keep their baseline guard because they replace a whole week.
    if(strncmp(str(get(args.p,"id")),"schedule:",9))cJSON_DeleteItemFromObjectCaseSensitive(args.p,"before");
    Owned prepared{prepare(args.p,owner)}; if(!prepared.p)return nullptr;
    copy(args.p,"token",get(prepared.p,"token"));
    return execute(args.p,owner,prepared.p);
}
bool control_observed(bool start, int mode) {
    return start?((mode>=1&&mode<=6)||mode==12||mode==23):(mode==0||mode==7);
}
const char *control_guard(const J *sensor, bool direct, bool identity, bool enabled, bool start) {
    if(!direct) return "START/STOP wymaga trybu RS485 bezposrednio";
    if(!enabled||!identity) return "Brak potwierdzonej tozsamosci lub nadawanie RS485 wylaczone";
    if(!cJSON_IsTrue(get(sensor,"fresh"))||num(get(sensor,"age_ms"))>5000||strcmp(str(get(get(sensor,"modules"),"module_a")),"8.41.38.L0")) return "Brak swiezego stanu i potwierdzenia firmware kotla";
    double mode=num(get(get(sensor,"sensors"),"state"));
    if(!std::isfinite(mode)||floor(mode)!=mode||mode<0||(mode>11&&mode!=12&&mode!=23)) return "Nierozpoznany stan kotla";
    if(start && (mode!=0||num(get(get(sensor,"sensors"),"pending_alerts"))!=0||!cJSON_IsFalse(get(get(sensor,"outputs"),"alarm")))) return "START wymaga wylaczonego kotla i braku zgloszonych alarmow";
    return nullptr;
}
const char *http_control_guard(const J *sys, const J *curr, const J *alarms, bool fresh, bool start) {
    if(!fresh) return "Brak swiezego odczytu stanu lub identyfikacji ecoNET";
    if(strcmp(str(get(sys,"controllerID")),"ecoMAX860P-LZ TOUCH")||strcmp(str(get(sys,"moduleASoftVer")),"8.41.38.L0")) return "Niezweryfikowany model lub firmware kotla";
    double mode=num(get(curr,"mode"));
    if(!std::isfinite(mode)||floor(mode)!=mode||mode<0||(mode>11&&mode!=12)) return "Nierozpoznany stan kotla";
    if(start&&(mode!=0||!cJSON_IsArray(alarms)||cJSON_GetArraySize(alarms)!=0||!cJSON_IsFalse(get(curr,"alarmOutputWorks")))) return "START wymaga wylaczonego kotla i braku zgloszonych alarmow";
    return nullptr;
}
const char *control_consistency(const J *sensor, double mode, bool start) {
    if(!cJSON_IsTrue(get(sensor,"fresh"))||num(get(sensor,"age_ms"))>5000) return nullptr;
    if(!equal(num(get(get(sensor,"sensors"),"state")),mode)) return "Stan RS485 i ecoNET jest niespojny. Poczekaj na aktualny odczyt";
    if(start&&(num(get(get(sensor,"sensors"),"pending_alerts"))!=0||!cJSON_IsFalse(get(get(sensor,"outputs"),"alarm")))) return "RS485 zglasza alarm lub brak potwierdzenia alarmow. START zablokowany";
    return nullptr;
}
bool inspect_control(bool start, bool direct, int &mode) {
    if(direct) {
        if(!rs_identify()) return false;
        Owned sensor{boiler::snapshot()};
        auto *guard=control_guard(sensor.p,true,native_identity&&now()-native_identity_ms.load()<60000,transport::enabled(),start);
        if(guard) return fail(guard);
        mode=static_cast<int>(num(get(get(sensor.p,"sensors"),"state"))); return true;
    }
    if(!refresh(true)) return false;
    Owned sensor{boiler::snapshot()};
    Lock lock;
    auto *curr=get(data,"curr");
    auto *guard=http_control_guard(get(data,"system"),curr,get(data,"alarms"),live_ms&&system_ms&&now()-live_ms<5000&&now()-system_ms<30000,start);
    if(guard) return fail(guard);
    guard=control_consistency(sensor.p,num(get(curr,"mode")),start);
    if(guard) return fail(guard);
    mode=static_cast<int>(num(get(curr,"mode"))); return true;
}
J *prepare_control(const J *payload, const std::string &owner) {
    bool start=!strcmp(str(get(payload,"action")),"start");
    int before=0; if(!inspect_control(start,selected_mode=="rs485",before)) return nullptr;
    auto *v=cJSON_CreateObject(); text(v,"kind","control"); text(v,"action",start?"start":"stop"); text(v,"name",start?"Wlaczenie kotla":"Wylaczenie kotla"); text(v,"transport",selected_mode);
    number(v,"before_mode",before); number(v,"value",start?1:0); number(v,"expires",epoch()+60); text(v,"token",random_token());
    cJSON_Delete(ticket); ticket=cJSON_Duplicate(v,true); ticket_owner=owner; ticket_deadline=now()+60000; ticket_revision=revision; return v;
}
J *execute_control(const J *payload, const std::string &owner) {
    Owned held{ticket}; ticket=nullptr;
    if(!held.p||strcmp(str(get(held.p,"kind")),"control")||strcmp(str(get(payload,"token")),str(get(held.p,"token")))||now()>ticket_deadline||revision!=ticket_revision||owner!=ticket_owner||!cJSON_IsTrue(get(payload,"confirm"))) { fail("Potwierdzenie sterowania wygaslo lub jest nieprawidlowe"); return nullptr; }
    if(selected_mode!=str(get(held.p,"transport"))) { fail("Droga sterowania zmienila sie. Przygotuj polecenie ponownie"); return nullptr; }
    bool start=num(get(held.p,"value"))==1, direct=selected_mode=="rs485"; auto started=now();
    phase("Sprawdzanie stanu kotla przed poleceniem","preflight");
    int before=0; if(!inspect_control(start,direct,before)) return nullptr;
    if(before!=num(get(held.p,"before_mode"))) { fail("Stan kotla zmienil sie. Przygotuj polecenie ponownie"); return nullptr; }
    auto *result=cJSON_CreateObject(); for(auto k:{"name","action","before_mode","transport"}) copy(result,k,get(held.p,k));
    text(result,"transport",direct?"rs485":"econet");
    if(!start&&control_observed(false,before)) { text(result,"state","unchanged"); text(result,"message",before==0?"Kociol jest juz wylaczony. Nie wyslano polecenia":"Kociol juz wygasza. Nie wyslano polecenia"); flag(result,"sent",false); log_result("control","bez_zapisu",result); return result; }
    number(result,"requested",start?1:0); text(result,"state","pending"); text(result,"message","Trwa pojedyncze polecenie START/STOP");
    { Lock lock; number(data,"write_count",num(get(data,"write_count"),0)+1); }
    log_result("control","pending",result);
    if(!journal()) { Lock lock; number(data,"write_count",num(get(data,"write_count"))-1); cJSON_DeleteItemFromArray(get(data,"tests"),0); cJSON_Delete(result); return nullptr; }
    phase(direct?"Wysylanie pojedynczego polecenia RS485":"Wysylanie pojedynczego polecenia do lokalnego ecoNET","dispatch");
    Owned baseline{boiler::snapshot()}; double frame_count=num(get(baseline.p,"accepted_frames"),0); auto sent=now();
    uint8_t value=start?1:0; transport::Reply reply;
    bool ack=false, dispatched=true;
    if(direct) { ack=transport::request(59,&value,1,187,reply)&&reply.length==10; dispatched=reply.sent; }
    else { Owned response{request("newParam",std::string("newParamName=BOILER_CONTROL&newParamValue=")+(start?"1":"0"),true)}; ack=response.p&&!strcmp(str(get(response.p,"result")),"OK"); }
    auto acknowledged=now();
    phase(ack?"Komenda przyjeta. Oczekiwanie na nowy stan kotla":"Brak ACK. Sprawdzanie stanu bez ponawiania","readback",&ack);
    unsigned matched=0; int observed=before; auto deadline=now()+(direct?15000:32000);
    auto *errors=cJSON_CreateArray();
    while(dispatched&&now()<deadline&&matched<2) {
        vTaskDelay(pdMS_TO_TICKS(direct?200:matched?500:1000));
        if(!direct) {
            if(!refresh()) { matched=0; cJSON_AddItemToArray(errors,cJSON_CreateString(error_text.c_str())); continue; }
            double mode=num(get(get(get(raw,"regParams"),"curr"),"mode"));
            if(!std::isfinite(mode)||floor(mode)!=mode||mode<0||(mode>11&&mode!=12)) { matched=0; continue; }
            observed=static_cast<int>(mode); matched=control_observed(start,observed)?matched+1:0; continue;
        }
        Owned current{boiler::snapshot()};
        if(!cJSON_IsTrue(get(current.p,"fresh"))) { matched=0; continue; }
        double count=num(get(current.p,"accepted_frames"),0);
        if(count<=frame_count || num(get(current.p,"updated_ms"),0)<=sent) continue;
        frame_count=count; observed=static_cast<int>(num(get(get(current.p,"sensors"),"state"),-1));
        matched=control_observed(start,observed)?matched+1:0;
    }
    const char *status=!dispatched?"not_sent":matched>=2?(ack?"verified":"observed_only"):ack?"acknowledged":"unverified";
    text(result,"state",status); flag(result,"sent",dispatched); flag(result,"ack_ok",ack); number(result,"observed_mode",observed); number(result,direct?"matched_frames":"matched_reads",matched); put(result,"read_errors",errors);
    text(result,"message",!dispatched?"Nie wyslano polecenia RS485":matched>=2?(start?"Odczytano rozpoczecie pracy kotla":observed==0?"Odczytano wylaczony kociol":"Odczytano wygaszanie. Kociol nie jest jeszcze wylaczony"):ack?"Komenda przyjeta, ale nowy stan niepotwierdzony. Nie ponowiono polecenia":"Brak potwierdzenia. Nie ponowiono polecenia. Sprawdz stan kotla");
    text(result,"verification_source",direct?"RS485 ECOMAX_CONTROL 59 / ACK 187 + nowe SENSOR_DATA 53":"ecoNET BOILER_CONTROL / result OK + dwa nowe odczyty regParams.mode");
    auto *timing=cJSON_CreateObject(); number(timing,"preflight",(sent-started)/1000.0); number(timing,direct?"write_rs485":"write_http",(acknowledged-sent)/1000.0); number(timing,"readback",(now()-acknowledged)/1000.0); number(timing,"total",(now()-started)/1000.0); put(result,"timing_seconds",timing);
    { Lock lock; if(!dispatched) number(data,"write_count",num(get(data,"write_count"))-1); auto *record=item(get(data,"tests"),0); J *v; cJSON_ArrayForEach(v,result) copy(record,v->string,v); text(record,"result",status); }
    if(!journal()) text(result,"journal_warning","Nie utrwalono wyniku w NVS");
    return result;
}
bool configure(const J *payload) {
    Config next{}; const char *host=str(get(payload,"host")), *user=str(get(payload,"username")), *password=str(get(payload,"password"));
    if(!private_ip(host)||strlen(host)>=sizeof(next.host)||!*user||strlen(user)>=sizeof(next.user)||!*password||strlen(password)>=sizeof(next.password)||strchr(user,':')) return fail("Podaj lokalny IPv4, login i haslo ecoNET");
    snprintf(next.host,sizeof(next.host),"%s",host); snprintf(next.user,sizeof(next.user),"%s",user); snprintf(next.password,sizeof(next.password),"%s",password);
    nvs_handle_t h; if(nvs_open("eco_panel",NVS_READWRITE,&h)!=ESP_OK) return fail("Brak dostepu do NVS");
    auto r=nvs_set_blob(h,"connection",&next,sizeof(next)); if(r==ESP_OK) r=nvs_commit(h); nvs_close(h); if(r!=ESP_OK) return fail("Nie zapisano konfiguracji");
    config=next; ++revision; cJSON_Delete(ticket); ticket=nullptr;
    cJSON_Delete(raw); raw=cJSON_CreateObject(); { Lock lock; text(data,"host",config.host); text(data,"username",config.user); flag(data,"requires_configuration",false); flag(data,"connected",false); live_ms=target_ms=system_ms=0; put(data,"targets",cJSON_CreateObject()); }
    return scan();
}
void sample_rs_history() {
    if(now()<next_history_sample)return;
    next_history_sample=now()+1000;
    Owned sensor{boiler::snapshot()}; if(!cJSON_IsTrue(get(sensor.p,"fresh")))return;
    Owned curr{cJSON_CreateObject()}; fill_rs_curr(curr.p,sensor.p);
    panel_history::append(curr.p,true,epoch(),now());
}
void sample_settings_history() {
    static uint64_t next=0;if(now()<next)return;next=now()+1000;
    Lock lock;if(!busy&&target_ms&&now()-target_ms<20000)panel_history::append_settings(data,selected_mode=="rs485",epoch(),now());
}
bool rs_alarm_refresh() {
    const uint8_t query[]={0,10}; transport::Reply reply; std::string error;
    if(!transport::request_readonly(61,query,2,189,reply))error="Brak odpowiedzi historii alarmow RS485";
    Owned parsed{error.empty()?protocol::describe(reply.bytes,reply.length):nullptr}; auto *details=get(parsed.p,"data");
    if(error.empty()&&(strcmp(str(get(parsed.p,"state")),"decoded")||num(get(details,"start"))!=0))error="Niezweryfikowany format historii alarmow RS485";
    Lock lock; auto *status=get(data,"alarm_status"); if(!cJSON_IsObject(status)) { status=cJSON_CreateObject(); put(data,"alarm_status",status); }
    text(status,"source","RS485 61/189"); text(status,"error",error);
    if(!error.empty())return false;
    copy(data,"alarms",get(details,"alerts")); native_alarm_ms=now(); number(status,"read_ms",native_alarm_ms);
    number(status,"read_epoch",epoch()); copy(status,"total_count",get(details,"total_count")); copy(status,"returned_count",get(details,"count"));
    flag(status,"history_available",true); flag(status,"complete",num(get(details,"count"))==num(get(details,"total_count"))); return true;
}
void worker(void *) {
    bool initial=true; uint64_t next_poll=0;
    for(;;) {
        std::string kind, owner; J *payload=nullptr;
        { Lock lock; if(!queued_kind.empty()) { kind=queued_kind; owner=queued_owner; payload=queued_payload; queued_kind.clear(); queued_payload=nullptr; } }
        if(!kind.empty()) {
            Owned args{payload}; error_text.clear(); J *result=nullptr; bool ok=true;
            if(kind=="prepare") result=prepare(payload,owner);
            else if(kind=="execute") result=execute(payload,owner);
            else if(kind=="save") result=save_setting(payload,owner);
            else if(kind=="control_prepare") result=prepare_control(payload,owner);
            else if(kind=="control_execute") result=execute_control(payload,owner);
            else if(kind=="transport") result=choose_transport(payload);
            else if(kind=="rs_probe") result=rs_probe(payload);
            else { ok=kind=="connect"?configure(payload):kind=="scan"?scan():refresh(true); if(ok) { result=cJSON_CreateObject(); text(result,"state","odczytane"); text(result,"message",selected_mode=="rs485"?"Odczytano dane RS485":"Odczytano dane lokalnego ecoNET"); } }
            { Lock lock; auto *job=get(data,"job"); number(job,"finished_ms",now()); if(result) { text(job,"status","done"); put(job,"result",result); } else { text(job,"status","failed"); text(job,"error",error_text); text(data,"error",error_text); } busy=false; }
            next_poll=now()+3000; initial=false;
        } else if(!busy && now()>=next_poll) {
            if(state::network().connected) {
                if(selected_mode=="rs485" && !transport::enabled() && !transport::conflict() && transport::enable()) { transport::Reply reply; transport::request(25,nullptr,0,0,reply); transport::request(85,nullptr,0,213,reply); }
                error_text.clear(); bool ok=selected_mode=="rs485"?rs_refresh(true):initial?scan():refresh();
                if(ok) { initial=false; }
                else { Lock lock; text(data,"error",error_text); }
                next_poll=now()+(initial?7000:3000);
            }
        }
        if(!busy&&selected_mode=="rs485")sample_rs_history();
        if(!busy)sample_settings_history();
        if(!busy&&selected_mode=="rs485"&&transport::enabled()&&native_identity&&now()-native_identity_ms<30000&&now()>=next_schedule_poll) {
            auto saved_error=error_text; bool ok=rs_schedule_refresh(); schedule_failures=ok?0:std::min(schedule_failures+1,2U);
            next_schedule_poll=now()+(ok?30000:std::min(60000U,15000U<<schedule_failures)); error_text=saved_error;
        }
        if(!busy&&selected_mode=="rs485"&&transport::enabled()&&native_identity&&now()-native_identity_ms<30000&&now()>=next_room_schedule_poll) {
            auto saved_error=error_text; bool ok=rs_room_schedule_refresh(); room_schedule_failures=ok?0:std::min(room_schedule_failures+1,2U);
            next_room_schedule_poll=now()+(ok?30000:std::min(60000U,15000U<<room_schedule_failures));error_text=saved_error;
        }
        if(!busy&&selected_mode=="rs485"&&transport::enabled()&&native_identity&&now()-native_identity_ms<30000&&state::network().connected) {
            Owned sensor{boiler::snapshot()}; int pending=static_cast<int>(num(get(get(sensor.p,"sensors"),"pending_alerts"),-1));
            if(now()>=next_alarm_poll||pending!=last_pending_alerts) {
                bool ok=rs_alarm_refresh(); alarm_failures=ok?0:std::min(alarm_failures+1,3U);
                last_pending_alerts=pending; next_alarm_poll=now()+(ok?30000:std::min(60000U,10000U<<alarm_failures));
            }
        }
        if(!busy && selected_mode=="rs485" && state::network().connected && now()>=next_health) {
            auto saved_error=error_text; Owned probe{request("regParams","",false,400)};
            if(probe.p&&!busy) { Owned network{request("regParamsData","",false,700)}; }
            health_failures=probe.p?0:std::min(health_failures+1,2U);
            unsigned delay=std::min(60000U,15000U<<health_failures); next_health=now()+delay;
            { Lock lock; number(get(data,"econet_health"),"retry_epoch",epoch()+delay/1000.0); }
            error_text=saved_error;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void fill_rs_curr(J *curr,const J *snapshot) {
    auto *s=get(snapshot,"sensors");
    const char *map[][2]={{"heating_temp","tempCO"},{"water_heater_temp","tempCWU"},{"outside_temp","tempExternalSensor"},{"feeder_temp","tempFeeder"},{"exhaust_temp","tempFlueGas"},{"heating_target","tempCOSet"},{"water_heater_target","tempCWUSet"},{"state","mode"},{"fan_power","fanPower"},{"boiler_load","boilerPower"},{"fuel_level","fuelLevel"},{"lambda_state","lambdaStatus"},{"heating_status","statusCO"},{"water_heater_status","statusCWU"}};
    for(auto &p:map) copy(curr,p[1],get(s,p[0]));
    copy(curr,"fuelStream",get(s,"fuel_consumption")); copy(curr,"boilerPowerKW",get(s,"boiler_power"));
    number(curr,"lambdaLevel",num(get(s,"lambda_level"))*10);
    const char *outputs[][2]={{"fan","fanWorks"},{"feeder","feederWorks"},{"heating_pump","pumpCOWorks"},{"water_heater_pump","pumpCWUWorks"},{"lighter","lighterWorks"},{"circulation_pump","pumpCirculationWorks"},{"alarm","alarmOutputWorks"}};
    for(auto &p:outputs) copy(curr,p[1],get(get(snapshot,"outputs"),p[0]));
    J *r; cJSON_ArrayForEach(r,get(snapshot,"thermostats")) { int i=static_cast<int>(num(get(r,"index")))+1; for(auto p:{std::pair<const char *,const char *>{"current_temp","ecoSterTemp"},{"target_temp","ecoSterSetTemp"},{"state","ecoSterMode"}}) { auto key=std::string(p.second)+std::to_string(i); copy(curr,key.c_str(),get(r,p.first)); } }
    cJSON_ArrayForEach(r,get(snapshot,"mixers")) { int i=static_cast<int>(num(get(r,"index")))+1; for(auto p:{std::pair<const char *,const char *>{"current_temp","mixerTemp"},{"target_temp","mixerSetTemp"},{"pump","mixerPumpWorks"}}) { auto key=std::string(p.second)+std::to_string(i); copy(curr,key.c_str(),get(r,p.first)); } }
}
void overlay(J *out) {
    const char *mode=str(get(out,"write_transport"));
    if(strcmp(mode,"rs485")&&strcmp(mode,"hybrid")) return;
    Owned b{boiler::snapshot()}; if(!cJSON_IsTrue(get(b.p,"fresh"))) return;
    auto *curr=get(out,"curr"); auto *s=get(b.p,"sensors");
    if(!strcmp(mode,"rs485")) {
        auto *alarms=get(out,"alarm_status"); if(!cJSON_IsObject(alarms)) { alarms=cJSON_CreateObject(); put(out,"alarm_status",alarms); }
        copy(alarms,"pending_count",get(s,"pending_alerts")); flag(alarms,"count_fresh",true);
        number(alarms,"count_age_seconds",num(get(b.p,"age_ms"))/1000.0);
    }
    fill_rs_curr(curr,b.p);
    // Types 8/53 can update faster than ecoNET's HTTP cache. Menu freshness stays separate.
    Owned latest{state::latest_frames(69,8,0,1,true)}; auto *frame=item(get(latest.p,"records"),0);
    if(frame && now()-num(get(frame,"ms"),0)<15000 && !strcmp(str(get(get(frame,"decoded"),"state")),"decoded")) {
        J *field; cJSON_ArrayForEach(field,get(get(get(frame,"decoded"),"data"),"fields")) { char key[24]; snprintf(key,sizeof(key),"%.0f",num(get(field,"id"))); copy(get(out,"values"),key,get(field,"value")); }
        copy(curr,"tempCWUSet",get(get(out,"values"),"1281")); copy(curr,"tempCOSet",get(get(out,"values"),"1280"));
    }
    for(auto &d:definitions) { auto *t=get(get(out,"targets"),d.id); if(t) { copy(t,"effective",get(curr,d.key)); number(t,"difference",num(get(t,"before"))-num(get(t,"effective"))); } }
    flag(out,"fresh",true); flag(out,"connected",true); flag(out,"rs485_connected",true); number(out,"age_seconds",num(get(b.p,"age_ms"))/1000.0); text(out,"live_source",!strcmp(mode,"hybrid")?"RS485 pasywny + lokalny ecoNET":"RS485 bezposrednio");
    number(out,"last_read_epoch",epoch()-num(get(b.p,"age_ms"))/1000.0); text(out,"last_read_utc",timestamp());
}
}
cJSON *snapshot() {
    J *out;
    { Lock lock; out=cJSON_Duplicate(data,true); flag(out,"busy",busy); number(out,"age_seconds",live_ms?(now()-live_ms)/1000.0:NAN); flag(out,"fresh",live_ms&&now()-live_ms<15000); flag(out,"targets_fresh",target_ms&&now()-target_ms<15000); number(out,"system_age_seconds",system_ms?(now()-system_ms)/1000.0:NAN);
      number(out,"target_read_ms",target_ms);
      auto *alarms=get(out,"alarm_status"); double read_ms=num(get(alarms,"read_ms"),0); flag(alarms,"history_fresh",read_ms&&now()-read_ms<65000); flag(alarms,"count_fresh",false);
      auto *health=get(out,"econet_health"); flag(health,"fresh",health_ms&&now()-health_ms<(selected_mode=="rs485"?65000:30000)); number(health,"age_seconds",health_ms?(now()-health_ms)/1000.0:NAN); }
    number(out,"rs485_identity_verified_ms",native_identity?native_identity_ms.load():0);
    bool http_fresh=cJSON_IsTrue(get(out,"fresh")); auto *health=get(out,"econet_health");
    bool econet_fresh=cJSON_IsTrue(get(health,"fresh"))&&cJSON_IsTrue(get(health,"api_ok"));
    flag(out,"econet_connected",econet_fresh); flag(out,"rs485_connected",false); flag(out,"connected",http_fresh);
    if(!http_fresh) { put(out,"curr",cJSON_CreateObject()); put(out,"values",cJSON_CreateObject()); }
    double http_mode=num(get(get(out,"curr"),"mode"));
    overlay(out);
    put(out,"pellet",pellet::snapshot());
    // The RS485 ecoNET address is emulated by ESP. Never label its network fields as real ecoNET data.
    auto *network=get(out,"econet_network"); if(!cJSON_IsObject(network)) { network=cJSON_CreateObject(); put(out,"econet_network",network); }
    bool network_fresh=econet_fresh&&econet_network_ms&&now()-econet_network_ms<65000;
    flag(network,"fresh",network_fresh); number(network,"age_seconds",econet_network_ms?(now()-econet_network_ms)/1000.0:NAN);
    if(!network_fresh)put(network,"values",cJSON_CreateObject());
    for(auto key:network_keys)copy(get(out,"values"),key,network_fresh?get(get(network,"values"),key):nullptr);
    auto net=state::network(); auto *esp=cJSON_CreateObject(); text(esp,"source","ESP32 Wi-Fi"); flag(esp,"connected",net.connected); text(esp,"ssid",net.ssid); text(esp,"ip",net.connected?net.ip:"");
    number(esp,"rssi_dbm",net.connected?net.rssi:NAN); number(esp,"channel",net.connected?net.channel:NAN); flag(esp,"mdns",net.mdns); flag(esp,"fixed",net.fixed); number(esp,"retries",net.retries); text(esp,"setup_ssid",net.ap);
    text(esp,"hostname",net.hostname); flag(esp,"hostname_pending",net.hostname_pending); number(esp,"hostname_error",net.hostname_error); put(out,"esp_network",esp);
    auto *schedule_status=get(out,"schedule_status"); bool direct_schedule=!strcmp(str(get(out,"write_transport")),"rs485");
    if(direct_schedule) { flag(schedule_status,"fresh",native_schedule_ms&&now()-native_schedule_ms<65000); number(schedule_status,"age_seconds",native_schedule_ms?(now()-native_schedule_ms)/1000.0:NAN); }
    auto *room_schedule_status=get(out,"thermostat_schedule_status");
    if(!room_schedule_status){room_schedule_status=cJSON_CreateObject();put(out,"thermostat_schedule_status",room_schedule_status);}
    if(direct_schedule){flag(room_schedule_status,"fresh",native_room_schedule_ms&&now()-native_room_schedule_ms<65000);number(room_schedule_status,"age_seconds",native_room_schedule_ms?(now()-native_room_schedule_ms)/1000.0:NAN);}
    Owned sensor{boiler::snapshot()}; auto *controls=cJSON_CreateObject();
    bool direct=!strcmp(str(get(out,"write_transport")),"rs485"), identity=native_identity&&now()-native_identity_ms.load()<60000;
    bool http_control_fresh=http_fresh&&num(get(out,"system_age_seconds"))<30;
    for(auto action:{"start","stop"}) { bool start=!strcmp(action,"start"); auto *guard=direct?control_guard(sensor.p,true,identity,transport::enabled(),start):http_control_guard(get(out,"system"),get(out,"curr"),get(out,"alarms"),http_control_fresh,start); if(!guard&&!direct) guard=control_consistency(sensor.p,http_mode,start); flag(controls,action,guard==nullptr); if(guard) text(controls,start?"start_error":"stop_error",guard); }
    copy(controls,"mode",direct?get(get(sensor.p,"sensors"),"state"):get(get(out,"curr"),"mode")); text(controls,"transport",direct?"rs485":"econet"); put(out,"controls",controls); return out;
}
bool is_busy() { return busy.load(); }
static J *completed_jobs=nullptr;
cJSON *job_snapshot(const char *id) {
    Lock lock; auto *job=get(data,"job");
    if(strcmp(id,str(get(job,"id")))) {
        job=nullptr; J *entry; cJSON_ArrayForEach(entry,completed_jobs)if(!strcmp(id,str(get(entry,"id")))){job=entry;break;}
        if(!job)return nullptr;
    }
    auto *out=cJSON_CreateObject(); copy(out,"job",job); flag(out,"busy",busy); text(out,"version",board::version); return out;
}
cJSON *submit(const char *kind, const cJSON *payload, const char *owner, std::string &error) {
    Lock lock;
    if(busy) { error="Trwa operacja. Poczekaj na zakonczenie"; return nullptr; }
    if(!strcmp(kind,"rs_probe")) {
        const char *family=str(get(payload,"family"));
        if(cJSON_GetArraySize(payload)!=1||(strcmp(family,"mixer")&&strcmp(family,"thermostat")&&strcmp(family,"boiler")&&strcmp(family,"menu")&&strcmp(family,"menu_point")&&strcmp(family,"menu_structure")&&strcmp(family,"schedules")&&strcmp(family,"thermostat_schedules")&&strcmp(family,"alarms"))) {
            error="Wybierz rodzine odczytu; surowe polecenia nie sa przyjmowane"; return nullptr;
        }
    }
    if(!strcmp(kind,"prepare")||!strcmp(kind,"save")) {
        const char *id=str(get(payload,"id"));
        if(strlen(id)>80||(!generic_id(id)&&!aligned(num(get(payload,"value")),definition(id)))) { error="Nieprawidlowy parametr lub krok zapisu"; return nullptr; }
    }
    if(!strcmp(kind,"control_prepare") && strcmp(str(get(payload,"action")),"start") && strcmp(str(get(payload,"action")),"stop")) { error="Nieprawidlowe polecenie sterowania"; return nullptr; }
    if((!strcmp(kind,"execute")||!strcmp(kind,"control_execute")) && (strlen(str(get(payload,"token")))!=32 || !cJSON_IsTrue(get(payload,"confirm")))) { error="Nieprawidlowe potwierdzenie zapisu"; return nullptr; }
    if(!strcmp(kind,"save")&&!cJSON_IsTrue(get(payload,"confirm"))) { error="Brak potwierdzenia zapisu"; return nullptr; }
    auto *previous=get(data,"job");
    if(previous&&strcmp(str(get(previous,"status")),"running")&&*str(get(previous,"id"))) {
        if(!completed_jobs)completed_jobs=cJSON_CreateArray();
        auto *history=completed_jobs;
        cJSON_InsertItemInArray(history,0,cJSON_Duplicate(previous,true));
        while(cJSON_GetArraySize(history)>6)cJSON_DeleteItemFromArray(history,6);
    }
    queued_kind=kind; queued_owner=owner; queued_payload=cJSON_Duplicate(payload,true);
    auto *job=cJSON_CreateObject(); text(job,"id",random_token()); text(job,"kind",kind); text(job,"status","running"); text(job,"step",""); text(job,"started",timestamp()); put(data,"job",job); busy=true;
    auto *out=cJSON_CreateObject(); copy(out,"job",job); return out;
}
bool self_test() {
    const auto saved_mode=selected_mode; selected_mode="econet";
    unsigned total=0, passed=0;
    auto check=[&](bool value) { ++total; if(value) ++passed; };
    check(private_ip("10.0.0.2")&&!private_ip("127.0.0.1")&&!private_ip("8.8.8.8")&&!private_ip("10.0.0.2/evil")&&!private_ip("10.0.0.999")&&definition("1281")&&!definition("9999"));
    auto *saved=raw;
    raw=cJSON_Parse(R"JSON({"sysParams":{"controllerID":"ecoMAX860P-LZ TOUCH","moduleASoftVer":"8.41.38.L0"},"rmCurrentDataParamsEdits":{"data":{"1281":{"type":4,"value":42,"min":35,"max":65}}},"regParams":{"curr":{"tempCWUSet":37}},"regParamsData":{"data":{"1281":37}},"rmStructure":{"data":[{"type":1,"index":47,"lock":false,"pass_index":0}]}})JSON");
    auto *names=cJSON_CreateArray(), *params=cJSON_CreateArray();
    for(unsigned i=0;i<48;++i) { cJSON_AddItemToArray(names,cJSON_CreateString(i==47?"Temperatura zadana CWU":"unused")); cJSON_AddItemToArray(params,cJSON_CreateObject()); }
    auto *wrapper=cJSON_CreateObject(); put(wrapper,"data",names); put(raw,"rmParamsNames",wrapper);
    wrapper=cJSON_CreateObject(); put(wrapper,"data",params); put(raw,"rmParamsData",wrapper);
    auto *m=item(params,47); number(m,"value",42); number(m,"minv",35); number(m,"maxv",65); flag(m,"edit",true); number(m,"unit",1); number(m,"mult",1); number(m,"offset",0);
    { Owned t{target("1281")}; check(t.p&&num(get(t.p,"before"))==42&&num(get(t.p,"effective"))==37); }
    auto reject=[&](J *parent,const char *key,J *bad) { Owned old{get(parent,key)?cJSON_Duplicate(get(parent,key),true):nullptr}; put(parent,key,bad); Owned t{target("1281")}; check(!t.p); copy(parent,key,old.p); };
    auto *sys=get(raw,"sysParams"), *meta=get(body("rmCurrentDataParamsEdits"),"1281"), *access=item(body("rmStructure"),0);
    reject(sys,"controllerID",cJSON_CreateString("unknown")); reject(sys,"moduleASoftVer",cJSON_CreateString("different"));
    reject(meta,"type",cJSON_CreateNumber(5)); reject(meta,"min",cJSON_CreateNumber(34)); reject(meta,"max",cJSON_CreateNumber(66)); reject(meta,"value",cJSON_CreateBool(true)); reject(meta,"value",cJSON_CreateNumber(70));
    auto *oldName=cJSON_DetachItemFromArray(names,47); cJSON_AddItemToArray(names,cJSON_CreateString("other")); { Owned t{target("1281")}; check(!t.p); } cJSON_DeleteItemFromArray(names,47); cJSON_AddItemToArray(names,oldName);
    reject(access,"lock",cJSON_CreateBool(true)); reject(access,"pass_index",cJSON_CreateNumber(1));
    cJSON_AddItemToArray(body("rmStructure"),cJSON_Duplicate(access,true)); { Owned t{target("1281")}; check(!t.p); } cJSON_DeleteItemFromArray(body("rmStructure"),1);
    reject(m,"value",cJSON_CreateNumber(43)); reject(m,"mult",cJSON_CreateNumber(10)); reject(m,"unit",cJSON_CreateNumber(31)); reject(m,"edit",cJSON_CreateBool(false)); reject(m,"maxv",cJSON_CreateNumber(64));
    reject(body("regParamsData"),"1281",cJSON_CreateNumber(38)); reject(body("regParamsData"),"1281",cJSON_CreateBool(true));
    for(unsigned i=48;i<=62;++i) { cJSON_AddItemToArray(names,cJSON_CreateString(i==62?"Temperatura zadana mieszacza 1":"unused")); cJSON_AddItemToArray(params,cJSON_CreateObject()); }
    put(body("rmCurrentDataParamsEdits"),"1287",cJSON_Parse(R"JSON({"type":4,"value":43,"min":40,"max":65})JSON"));
    auto *mix=item(params,62); number(mix,"value",43); number(mix,"minv",40); number(mix,"maxv",65); flag(mix,"edit",true); number(mix,"unit",1); number(mix,"mult",1); number(mix,"offset",0);
    cJSON_AddItemToArray(body("rmStructure"),cJSON_Parse(R"JSON({"type":1,"index":62,"lock":false,"pass_index":0})JSON"));
    number(get(get(raw,"regParams"),"curr"),"mixerSetTemp1",43); number(body("regParamsData"),"1287",43);
    { Owned t{target("1287")}; check(t.p&&num(get(t.p,"step"))==1&&num(get(t.p,"before"))==43); }
    wrapper=cJSON_CreateObject(); put(wrapper,"data",cJSON_Parse(R"JSON({"STER_MODE_1":{"value":2,"sec":36,"pos":1,"unit":31},"STER_TEMP_DAY_1":{"value":22.7,"minv":10,"maxv":35,"unit":1,"mult":0.1,"sec":36,"pos":2,"edit":true},"STER_TEMP_NIGHT_1":{"value":26.3,"minv":10,"maxv":35,"unit":1,"mult":0.1,"sec":36,"pos":3,"edit":true}})JSON")); put(raw,"editParams",wrapper);
    auto *curr=get(get(raw,"regParams"),"curr"), *room=get(body("editParams"),"STER_TEMP_DAY_1");
    number(curr,"ecoSterTemp1",23.4); number(curr,"ecoSterSetTemp1",22.69999885559082); number(curr,"ecoSterMode1",2);
    { Owned t{target("STER_TEMP_DAY_1")}; check(t.p&&cJSON_IsTrue(get(t.p,"active_profile"))&&equal(num(get(t.p,"step")),0.1)); }
    { Owned t{target("STER_TEMP_NIGHT_1")}; check(!t.p); }
    number(curr,"ecoSterSetTemp1",23); { Owned t{target("STER_TEMP_DAY_1")}; check(t.p&&num(get(t.p,"before"))==23&&!cJSON_IsTrue(get(t.p,"metadata_in_sync"))); } number(curr,"ecoSterSetTemp1",22.69999885559082);
    check(aligned(22.8,definition("STER_TEMP_DAY_1"))&&!aligned(22.75,definition("STER_TEMP_DAY_1"))&&!aligned(43.1,definition("1287"))&&!aligned(NAN,definition("1287")));
    check(wire_value(23)=="23"&&wire_value(22.8)=="22.8"&&equal(22.7,22.69999885559082)&&!equal(22.7,22.8));
    check(definition("1287")->native&&definition("STER_TEMP_DAY_1")->native&&definition("STER_TEMP_NIGHT_1")->native&&definition("1281")->native&&
          definition("STER_TEMP_SET_PARTY_1")->native&&definition("STER_TEMP_SET_SUMMER_1")->native);
    check(aligned(23.2,definition("STER_TEMP_SET_PARTY_1"))&&!aligned(23.25,definition("STER_TEMP_SET_PARTY_1")));
    { Owned t{target("STER_TEMP_SET_PARTY_1")}; check(!t.p); }
    auto reject_room=[&](J *parent,const char *key,J *bad) { Owned old{get(parent,key)?cJSON_Duplicate(get(parent,key),true):nullptr}; put(parent,key,bad); Owned t{target("STER_TEMP_DAY_1")}; check(!t.p); copy(parent,key,old.p); };
    reject_room(room,"edit",cJSON_CreateBool(false)); reject_room(room,"unit",cJSON_CreateNumber(31)); reject_room(room,"mult",cJSON_CreateNumber(1));
    reject_room(room,"sec",cJSON_CreateNumber(35)); reject_room(room,"pos",cJSON_CreateNumber(3)); reject_room(room,"minv",cJSON_CreateNumber(9));
    reject_room(room,"maxv",cJSON_CreateNumber(36)); reject_room(room,"value",cJSON_CreateNumber(22.75));
    reject_room(curr,"ecoSterTemp1",cJSON_CreateNull()); reject_room(curr,"ecoSterMode1",cJSON_CreateNumber(3)); reject_room(curr,"ecoSterSetTemp1",cJSON_CreateNumber(36));
    flag(item(body("rmStructure"),1),"lock",true); { Owned t{target("1287")}; check(!t.p); }
    // Generic parameter tests use only in-memory fixtures; they cannot dispatch HTTP.
    Owned menu_versions{cJSON_Parse(R"JSON({"command":0,"minor":0,"major":1,"commands":[{"command":2,"version":7672},{"command":3,"version":51616},{"command":6,"version":13175},{"command":7,"version":7227},{"command":8,"version":15074},{"command":9,"version":31602},{"command":13,"version":50115},{"command":14,"version":373}]})JSON")};
    check(menu_versions_match(menu_versions.p));
    auto *versions=get(menu_versions.p,"commands");
    for(int i=0;i<cJSON_GetArraySize(versions);++i) { auto *entry=item(versions,i); double old=num(get(entry,"version")); number(entry,"version",old+1); check(menu_versions_match(menu_versions.p)==(num(get(entry,"command"))==9)); number(entry,"version",old); }
    cJSON_AddItemToArray(versions,cJSON_Duplicate(item(versions,0),true)); check(!menu_versions_match(menu_versions.p)); cJSON_DeleteItemFromArray(versions,8);
    number(menu_versions.p,"major",2); check(!menu_versions_match(menu_versions.p)); number(menu_versions.p,"major",1);
    Owned encoding{cJSON_Parse(R"JSON({"min":-10,"max":10,"step":0.1,"wire_unit":1,"wire_mult":0.1,"wire_offset":100})JSON")}; uint8_t encoded=0;
    check(encode_menu_value(encoding.p,0.9,encoded)&&encoded==109); check(encode_menu_value(encoding.p,-0.9,encoded)&&encoded==91);
    check(!encode_menu_value(encoding.p,0.95,encoded)&&!encode_menu_value(encoding.p,NAN,encoded));
    number(encoding.p,"wire_mult",0); check(!encode_menu_value(encoding.p,1,encoded)); number(encoding.p,"wire_mult",0.1);
    number(encoding.p,"wire_offset",300); check(!encode_menu_value(encoding.p,1,encoded));
    number(encoding.p,"min",0); number(encoding.p,"max",7); number(encoding.p,"step",1); number(encoding.p,"wire_unit",31);
    check(encode_menu_value(encoding.p,5,encoded)&&encoded==5&&!encode_menu_value(encoding.p,8,encoded));
    auto *structure=body("rmStructure"); cJSON_DeleteItemFromArray(structure,1);
    cJSON_InsertItemInArray(structure,0,cJSON_Parse(R"JSON({"type":0,"index":0,"lock":false,"pass_index":0})JSON"));
    { Owned t{generic_target("rm:47")}; check(t.p&&generic_value(t.p,43)&&!generic_value(t.p,43.5)&&!generic_value(t.p,70)&&!generic_value(t.p,NAN)); }
    { Owned t{generic_target("rm:047")}; check(!t.p); }
    flag(item(structure,0),"lock",true); { Owned t{generic_target("rm:47")}; check(!t.p); } flag(item(structure,0),"lock",false);
    number(item(structure,0),"pass_index",2); { Owned t{generic_target("rm:47")}; check(!t.p); } number(item(structure,0),"pass_index",1);
    { Owned t{generic_target("rm:47")}; check(t.p&&cJSON_IsTrue(get(t.p,"risk"))); } number(item(structure,0),"pass_index",0);
    auto *mode=get(body("editParams"),"STER_MODE_1"); number(mode,"mult",19); number(mode,"minv",0); number(mode,"maxv",7); flag(mode,"edit",true);
    { Owned t{generic_target("legacy:STER_MODE_1")}; check(t.p&&cJSON_GetArraySize(get(t.p,"options"))==8&&generic_value(t.p,5)&&!generic_value(t.p,8)&&!generic_value(t.p,2.5)&&!cJSON_IsTrue(get(t.p,"cached"))); }
    number(curr,"ecoSterMode1",133); { Owned t{generic_target("legacy:STER_MODE_1")}; check(t.p&&num(get(t.p,"before"))==5&&num(get(t.p,"profile_raw"))==133); } number(curr,"ecoSterMode1",2);
    Owned invalid_mode{cJSON_CreateNumber(255)}; check(!std::isfinite(room_mode(invalid_mode.p))); number(curr,"ecoSterMode1",130); { Owned t{target("STER_TEMP_DAY_1")}; check(t.p&&num(get(t.p,"profile_mode"))==2); } number(curr,"ecoSterMode1",2);
    { Owned t{generic_target("legacy:STER_TEMP_NIGHT_1")}; check(t.p&&cJSON_IsTrue(get(t.p,"cached"))); }
    number(curr,"ecoSterMode1",1); number(curr,"ecoSterSetTemp1",21.5);
    { Owned t{target("STER_TEMP_NIGHT_1")}; check(t.p&&equal(num(get(t.p,"before")),21.5)); }
    { Owned t{generic_target("legacy:STER_TEMP_NIGHT_1")}; check(t.p&&!cJSON_IsTrue(get(t.p,"cached"))&&equal(num(get(t.p,"before")),21.5)); }
    number(mode,"pos",2); { Owned t{generic_target("legacy:STER_MODE_1")}; check(!t.p); } number(mode,"pos",1);
    { Owned t{generic_target("legacy:STER_UNKNOWN_1")}; check(!t.p); }
    { Owned t{generic_target("legacy:STER_MODE_2")}; check(!t.p); }
    Owned week{cJSON_Parse("[[128,0,0,0,0,1],[0,0,0,0,0,0],[0,0,0,0,0,0],[0,0,0,0,0,0],[0,0,0,0,0,0],[0,0,0,0,0,0],[0,0,0,0,0,0],[1,5,0,20]]")};
    auto *schedules=cJSON_CreateObject(), *em=cJSON_CreateObject(); put(schedules,"ecomaxSchedules",em); copy(em,"cwuTZ",week.p); put(sys,"schedules",schedules);
    { Owned t{schedule_target("schedule:cwuTZ")}; check(t.p&&schedule_value(t.p,week.p)); }
    { Owned t{schedule_target("schedule:cwuTZ")}; uint8_t bytes[46]{}; number(t.p,"wire_index",1);
      check(encode_schedule(t.p,week.p,bytes)&&bytes[0]==1&&bytes[1]==1&&bytes[2]==1&&bytes[3]==5&&bytes[4]==128&&bytes[9]==1&&bytes[10]==0&&bytes[45]==0);
      number(t.p,"wire_index",2); check(!encode_schedule(t.p,week.p,bytes)); number(t.p,"wire_index",1);
      text(t.p,"wire_key","missing"); check(!encode_schedule(t.p,week.p,bytes));
    }
    check(rs_schedule_index("boilerTZ")==0&&rs_schedule_index("cwuTZ")==1&&rs_schedule_index("mixer1TZ")==6&&rs_schedule_index("thermostat1TZ")==16);
    { Owned t{cJSON_CreateObject()},w{cJSON_Duplicate(week.p,true)}; auto *m=item(w.p,7);
      for(int i=1;i<4;++i)cJSON_ReplaceItemInArray(m,i,cJSON_CreateNumber(4));
      copy(t.p,"before",w.p);text(t.p,"wire_key","thermostat1TZ");number(t.p,"wire_index",16);number(t.p,"wire_parameter",4);
      uint8_t bytes[46]{};check(encode_schedule(t.p,w.p,bytes)&&bytes[1]==16&&bytes[3]==4);
      number(t.p,"wire_parameter",5);check(!encode_schedule(t.p,w.p,bytes));
      number(t.p,"wire_parameter",3);for(unsigned i=1;i<4;++i)cJSON_SetNumberValue(item(item(w.p,7),i),3);copy(t.p,"before",w.p);
      check(encode_schedule(t.p,w.p,bytes)&&bytes[3]==3);
      number(t.p,"wire_parameter",101);check(!encode_schedule(t.p,w.p,bytes)); }
    check(rs_schedule_index("cwuTZ/evil")==-1&&rs_schedule_index("1")==-1);
    { Owned t{schedule_target("schedule:missing")}; check(!t.p); }
    check(valid_schedule(week.p)); cJSON_ReplaceItemInArray(item(week.p,0),0,cJSON_CreateNumber(256)); check(!valid_schedule(week.p)); cJSON_ReplaceItemInArray(item(week.p,0),0,cJSON_CreateNumber(128));
    cJSON_ReplaceItemInArray(item(week.p,7),0,cJSON_CreateNumber(2)); check(!valid_schedule(week.p)); cJSON_ReplaceItemInArray(item(week.p,7),0,cJSON_CreateNumber(1));
    cJSON_ReplaceItemInArray(item(week.p,7),3,cJSON_CreateNumber(21)); { Owned t{schedule_target("schedule:cwuTZ")}; check(t.p&&!schedule_value(t.p,week.p)); }
    wrapper=cJSON_CreateObject(); put(wrapper,"data",cJSON_Parse("[\"Menu\",\"Serwis\"]")); put(raw,"rmCatsNames",wrapper);
    Owned dynamic_structure{cJSON_Parse(R"JSON([{"type":7,"index":1,"lock":false,"pass_index":0},{"type":0,"index":0,"lock":false,"pass_index":0},{"type":7,"index":1,"lock":false,"pass_index":0},{"type":0,"index":1,"lock":false,"pass_index":1},{"type":7,"index":1,"lock":false,"pass_index":0},{"type":1,"index":47,"lock":false,"pass_index":0}])JSON")};
    check(menu_structure_valid(dynamic_structure.p));
    wrapper=cJSON_CreateObject(); copy(wrapper,"data",dynamic_structure.p); put(raw,"rmStructure",wrapper); check(menu_access(47));
    flag(item(body("rmStructure"),3),"lock",true); check(!menu_access(47)); flag(item(body("rmStructure"),3),"lock",false);
    number(item(body("rmStructure"),3),"pass_index",2); check(!menu_access(47));
    number(item(dynamic_structure.p,5),"index",168); check(!menu_structure_valid(dynamic_structure.p)); number(item(dynamic_structure.p,5),"index",47);
    number(item(dynamic_structure.p,3),"pass_index",8); check(!menu_structure_valid(dynamic_structure.p)); number(item(dynamic_structure.p,3),"pass_index",1);
    number(item(dynamic_structure.p,3),"index",2); check(!menu_structure_valid(dynamic_structure.p));
    Owned missing_parent{cJSON_Parse(R"JSON([{"type":1,"index":47,"lock":false,"pass_index":0}])JSON")}; check(!menu_structure_valid(missing_parent.p));
    cJSON_Delete(raw); raw=saved; error_text.clear();
    Owned sensor{cJSON_Parse(R"JSON({"fresh":true,"age_ms":100,"modules":{"module_a":"8.41.38.L0"},"sensors":{"state":0,"pending_alerts":0},"outputs":{"alarm":false}})JSON")};
    auto guarded=[&](bool start) { return control_guard(sensor.p,true,true,true,start); };
    check(!guarded(true)&&!guarded(false));
    check(control_guard(sensor.p,false,true,true,true)!=nullptr);
    check(control_guard(sensor.p,true,false,true,true)!=nullptr);
    check(control_guard(sensor.p,true,true,false,true)!=nullptr);
    flag(sensor.p,"fresh",false); check(guarded(true)!=nullptr); flag(sensor.p,"fresh",true);
    number(sensor.p,"age_ms",5001); check(guarded(true)!=nullptr); number(sensor.p,"age_ms",100);
    text(get(sensor.p,"modules"),"module_a","unknown"); check(guarded(true)!=nullptr); text(get(sensor.p,"modules"),"module_a","8.41.38.L0");
    auto *s=get(sensor.p,"sensors"), *outputs=get(sensor.p,"outputs");
    number(s,"state",12); check(!guarded(false)&&guarded(true)!=nullptr);
    number(s,"state",23); check(!guarded(false)&&guarded(true)!=nullptr);
    number(s,"state",99); check(guarded(false)!=nullptr);
    put(s,"state",nullptr); check(guarded(false)!=nullptr);
    number(s,"state",3); check(guarded(true)!=nullptr&&!guarded(false)); number(s,"state",0);
    number(s,"pending_alerts",1); check(guarded(true)!=nullptr&&!guarded(false)); number(s,"pending_alerts",0);
    flag(outputs,"alarm",true); check(guarded(true)!=nullptr); flag(outputs,"alarm",false);
    put(outputs,"alarm",nullptr); check(guarded(true)!=nullptr);
    check(control_observed(true,2)&&control_observed(true,6)&&!control_observed(true,7)&&!control_observed(true,8)&&!control_observed(true,0)&&control_observed(false,0)&&control_observed(false,7)&&!control_observed(false,3));
    Owned http_sys{cJSON_Parse(R"JSON({"controllerID":"ecoMAX860P-LZ TOUCH","moduleASoftVer":"8.41.38.L0"})JSON")};
    Owned http_curr{cJSON_Parse(R"JSON({"mode":0,"alarmOutputWorks":false})JSON")}; Owned http_alarms{cJSON_CreateArray()};
    auto http_guarded=[&](bool start) { return http_control_guard(http_sys.p,http_curr.p,http_alarms.p,true,start); };
    check(!http_guarded(true)&&!http_guarded(false));
    check(http_control_guard(http_sys.p,http_curr.p,http_alarms.p,false,true)!=nullptr);
    text(http_sys.p,"controllerID","other"); check(http_guarded(true)!=nullptr); text(http_sys.p,"controllerID","ecoMAX860P-LZ TOUCH");
    text(http_sys.p,"moduleASoftVer","other"); check(http_guarded(true)!=nullptr); text(http_sys.p,"moduleASoftVer","8.41.38.L0");
    put(http_curr.p,"mode",nullptr); check(http_guarded(false)!=nullptr);
    for(double mode:{-1.0,3.5,99.0}) { number(http_curr.p,"mode",mode); check(http_guarded(false)!=nullptr); }
    number(http_curr.p,"mode",12);check(!http_guarded(false)&&http_guarded(true)!=nullptr);
    check(control_observed(true,12)&&control_observed(true,23));
    number(http_curr.p,"mode",3); check(http_guarded(true)!=nullptr&&!http_guarded(false)); number(http_curr.p,"mode",0);
    flag(http_curr.p,"alarmOutputWorks",true); check(http_guarded(true)!=nullptr&&!http_guarded(false)); put(http_curr.p,"alarmOutputWorks",nullptr); check(http_guarded(true)!=nullptr); flag(http_curr.p,"alarmOutputWorks",false);
    cJSON_AddItemToArray(http_alarms.p,cJSON_CreateObject()); check(http_guarded(true)!=nullptr&&!http_guarded(false)); cJSON_DeleteItemFromArray(http_alarms.p,0);
    check(http_control_guard(http_sys.p,http_curr.p,nullptr,true,true)!=nullptr);
    check(!http_guarded(true)&&!http_guarded(false));
    flag(outputs,"alarm",false); check(!control_consistency(sensor.p,0,true));
    check(control_consistency(sensor.p,3,true)!=nullptr); number(s,"pending_alerts",1); check(control_consistency(sensor.p,0,true)!=nullptr); number(s,"pending_alerts",0);
    flag(outputs,"alarm",true); check(control_consistency(sensor.p,0,true)!=nullptr); flag(outputs,"alarm",false);
    flag(sensor.p,"fresh",false); check(!control_consistency(sensor.p,3,true)); flag(sensor.p,"fresh",true);
    {
        Owned target{cJSON_CreateObject()},payload{cJSON_CreateObject()}; number(target.p,"before",22); flag(target.p,"cached",false);
        check(allow_same_value_audit(target.p,payload.p)&&!get(target.p,"audit_same_value"));
        flag(payload.p,"audit_same_value",true);number(payload.p,"before",22);number(payload.p,"value",22);flag(payload.p,"confirm_audit_same_value",true);
        check(!allow_same_value_audit(target.p,payload.p));selected_mode="rs485";
        check(allow_same_value_audit(target.p,payload.p)&&cJSON_IsTrue(get(target.p,"audit_same_value")));
        number(payload.p,"value",22.1);check(!allow_same_value_audit(target.p,payload.p));number(payload.p,"value",22);
        flag(payload.p,"confirm_audit_same_value",false);check(!allow_same_value_audit(target.p,payload.p));flag(payload.p,"confirm_audit_same_value",true);
        flag(target.p,"cached",true);check(!allow_same_value_audit(target.p,payload.p));flag(target.p,"cached",false);
        cJSON_DeleteItemFromObjectCaseSensitive(payload.p,"before");check(!allow_same_value_audit(target.p,payload.p));
    }
    state::log("PANEL WRITE GUARD SELF-TEST: %u/%u %s; offline fixtures, no write requests",passed,total,passed==total?"PASS":"FAIL");
    selected_mode=saved_mode;
    return passed==total;
}
void set_clock(double seconds) {
    if(!std::isfinite(seconds)||seconds<1577836800||seconds>4102444800) return;
    struct timeval tv{}; tv.tv_sec=static_cast<time_t>(seconds); settimeofday(&tv,nullptr);
    Lock lock; number(data,"system_read_epoch",system_ms?epoch()-(now()-system_ms)/1000.0:0); number(data,"target_read_epoch",target_ms?epoch()-(now()-target_ms)/1000.0:0);
    if(live_ms) { number(data,"last_read_epoch",epoch()-(now()-live_ms)/1000.0); text(data,"last_read_utc",timestamp()); }
    auto *series=get(data,"series"); if(cJSON_GetArraySize(series) && num(get(item(series,0),"time"),0)<1577836800) put(data,"series",cJSON_CreateArray());
}
void start() {
    panel_history::start();
    mutex=xSemaphoreCreateMutex(); configASSERT(mutex); raw=cJSON_Parse(panel_catalog_json); configASSERT(raw); data=cJSON_CreateObject();
    snprintf(config.host,sizeof(config.host),"%s",econet_provision::host); snprintf(config.user,sizeof(config.user),"%s",econet_provision::user); snprintf(config.password,sizeof(config.password),"%s",econet_provision::password);
    nvs_handle_t h;
    if(nvs_open("eco_panel",NVS_READONLY,&h)==ESP_OK) {
        char mode[12]{}; size_t mode_size=sizeof(mode); if(nvs_get_str(h,"transport",mode,&mode_size)==ESP_OK && (!strcmp(mode,"rs485")||!strcmp(mode,"hybrid")||!strcmp(mode,"econet"))) selected_mode=mode;
        char identity[161]{}; size_t identity_size=sizeof(identity); if(nvs_get_str(h,"identity",identity,&identity_size)==ESP_OK && identity_size>1) uid=identity;
        Config saved{}; size_t size=sizeof(saved); if(nvs_get_blob(h,"connection",&saved,&size)==ESP_OK && size==sizeof(saved)) { saved.host[15]=saved.user[63]=saved.password[95]=0; if(private_ip(saved.host)) config=saved; }
        size=0; if(nvs_get_blob(h,"journal",nullptr,&size)==ESP_OK && size>1 && size<6500) { std::string journal(size,'\0'); if(nvs_get_blob(h,"journal",journal.data(),&size)==ESP_OK) { Owned j{cJSON_Parse(journal.c_str())}; if(cJSON_IsArray(get(j.p,"tests"))) copy(data,"tests",get(j.p,"tests")); if(cJSON_IsNumber(get(j.p,"write_count"))) copy(data,"write_count",get(j.p,"write_count")); } }
        nvs_close(h);
    }
    for(auto key:{"system","curr","values","names","endpoints","edits","targets","target_errors","versions","legacy_params"}) put(data,key,cJSON_CreateObject());
    for(auto key:{"menu","units","enums","categories","structure","category_names","category_descriptions","tiles","series"}) put(data,key,cJSON_CreateArray());
    if(!get(data,"tests")) put(data,"tests",cJSON_CreateArray());
    if(!get(data,"write_count")) number(data,"write_count",0);
    J *record; cJSON_ArrayForEach(record,get(data,"tests")) if(!strcmp(str(get(record,"state")),"pending")) { text(record,"state","interrupted"); text(record,"result","interrupted"); text(record,"message","Restart po rozpoczeciu zapisu. Wynik nieznany; nie ponowiono polecenia"); }
    text(data,"version",board::version); text(data,"host",config.host); text(data,"username",config.user); text(data,"write_transport",selected_mode); text(data,"live_source","RS485 / lokalny ecoNET");
    auto *health=cJSON_CreateObject(); text(health,"host",config.host); put(health,"available",nullptr); put(health,"api_ok",nullptr); put(data,"econet_health",health);
    flag(data,"connected",false); for(auto key:{"last_read_utc","latency_ms","error","alarms","schedules","job"}) put(data,key,nullptr);
    flag(data,"requires_configuration",!*config.user||!*config.password);
    number(data,"last_read_epoch",0); number(data,"target_read_epoch",0); number(data,"system_read_epoch",0);
    copy(data,"system",get(raw,"sysParams")); copy(data,"tiles",get(get(raw,"sysParams"),"tiles")); build_menu();
    auto *allowed=cJSON_CreateArray(); for(auto &d:definitions) cJSON_AddItemToArray(allowed,cJSON_CreateString(d.id)); put(data,"write_allowlist",allowed);
    configASSERT(self_test()); configASSERT(xTaskCreate(worker,"econet_panel",16384,nullptr,3,nullptr)==pdPASS);
    state::log("Autonomous panel ready; local ecoNET HTTP writes, RS485 TX remains locked");
}
}
