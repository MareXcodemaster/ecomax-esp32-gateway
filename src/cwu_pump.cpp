#include "cwu_pump.hpp"
#include "circulation_policy.hpp"
#include "tapo_discovery.hpp"
#include "third_party/esp_tapo/tapo_client.h"
#include "boiler.hpp"
#include "panel.hpp"
#include "wifi.hpp"
#include "state.hpp"
#include "updates.hpp"
#include "nvs.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>

namespace cwu_pump {
namespace {
using esphome::tapo::TapoClient;
using esphome::tapo::TapoCipher;
using esphome::tapo::TapoTransport;
struct Config {
    uint32_t magic=0x43575031,revision=0;
    circulation::Rules rules;
    bool credentials=false,armed=false;
    char host[16]{},identity[65]{},mac[13]{};
    uint8_t auth[32]{};
    char requests[8][33]{};
    uint8_t request_cursor=0;
};
Config config;
SemaphoreHandle_t mutex,config_mutex;
TaskHandle_t worker=nullptr;
bool known=false,actual_on=false,linked=false,pending=false;
uint64_t verified_ms=0,last_off_ms=0;
unsigned writes=0;
std::string status="Nie skonfigurowano",alias,model;
circulation::Decision decision;
std::vector<tapo_discovery::Device> discovered;
bool scan_requested=false,scan_busy=false;
unsigned scan_sequence=0;
std::string scan_request_id;
struct Lock {SemaphoreHandle_t m;Lock(SemaphoreHandle_t v):m(v){xSemaphoreTake(m,portMAX_DELAY);}~Lock(){xSemaphoreGive(m);}};
uint64_t now(){return esp_timer_get_time()/1000;}
const cJSON *get(const cJSON *v,const char *key){return cJSON_GetObjectItemCaseSensitive(v,key);}
std::string text(const cJSON *v){return cJSON_IsString(v)?v->valuestring:"";}
bool private_ip(const char *value){
    unsigned a,b,c,d;char tail;
    return sscanf(value,"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)==4&&a<=255&&b<=255&&c<=255&&d>0&&d<255&&
        (a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168));
}
std::vector<uint8_t> bytes(const std::string &s){return {s.begin(),s.end()};}
std::string identity(const std::string &s){return TapoCipher::to_hex_string(TapoCipher::sha256(bytes(s)));}
bool store(const Config &c){
    nvs_handle_t h;if(nvs_open("cwu_pump",NVS_READWRITE,&h)!=ESP_OK)return false;
    auto e=nvs_set_blob(h,"config_v1",&c,sizeof(c));if(e==ESP_OK)e=nvs_commit(h);nvs_close(h);return e==ESP_OK;
}
class Transport:public TapoTransport {
public:
    std::string host;
    void reset()override{}
    Response post(const std::string &path,const std::string &body,const std::string &cookie)override{
        Response out;if(!private_ip(host.c_str())||path.rfind("/app/",0)!=0)return out;
        std::string url="http://"+host+path;
        esp_http_client_config_t cfg{};cfg.url=url.c_str();cfg.timeout_ms=2500;cfg.disable_auto_redirect=true;cfg.buffer_size=512;
        cfg.event_handler=[](esp_http_client_event_t *event)->esp_err_t{
            auto *response=static_cast<Response *>(event->user_data);
            if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&!strcasecmp(event->header_key,"Set-Cookie")){
                if(response->set_cookie.size()+strlen(event->header_value)+2<384){if(!response->set_cookie.empty())response->set_cookie+="; ";response->set_cookie+=event->header_value;}
            }
            return ESP_OK;
        };
        cfg.user_data=&out;auto client=esp_http_client_init(&cfg);if(!client)return out;
        esp_http_client_set_method(client,HTTP_METHOD_POST);esp_http_client_set_header(client,"Content-Type","application/octet-stream");
        if(!cookie.empty())esp_http_client_set_header(client,"Cookie",cookie.c_str());
        auto deadline=esp_timer_get_time()+5000000;
        if(esp_http_client_open(client,body.size())==ESP_OK){
            int sent=0;while(sent<static_cast<int>(body.size())){int n=esp_http_client_write(client,body.data()+sent,body.size()-sent);if(n<=0)break;sent+=n;}
            if(sent==static_cast<int>(body.size())&&esp_http_client_fetch_headers(client)>=0){
                out.status=esp_http_client_get_status_code(client);char buffer[512];
                while(out.body.size()<8192&&esp_timer_get_time()<deadline){int n=esp_http_client_read(client,buffer,std::min(sizeof(buffer),8192-out.body.size()));if(n<0){out.status=-1;break;}if(!n)break;out.body.append(buffer,n);}
                if(esp_timer_get_time()>=deadline)out.status=-1;
                if(out.body.size()>=8192)out.status=-1;
            }
        }
        esp_http_client_close(client);esp_http_client_cleanup(client);return out;
    }
};
bool call(TapoClient &client,const std::string &message,cJSON *&result){
    auto response=client.send_message(message);result=response.empty()?nullptr:cJSON_ParseWithLength(response.data(),response.size());
    const auto *code=get(result,"error_code");
    return cJSON_IsObject(result)&&cJSON_IsNumber(code)&&code->valuedouble==0;
}
bool read(TapoClient &client,const Config &c,std::string &found_id,bool &on,std::string &name,std::string &type,std::string &mac){
    cJSON *body=nullptr;bool ok=call(client,R"JSON({"method":"get_device_info"})JSON",body);
    const auto *r=get(body,"result"),*state=get(r,"device_on");
    type=text(get(r,"model"));auto id=text(get(r,"device_id"));
    ok=ok&&type=="P110"&&!id.empty()&&id.size()<160&&cJSON_IsBool(state);
    found_id=identity(id);if(*c.identity&&found_id!=c.identity)ok=false;
    mac=tapo_discovery::normalize_mac(text(get(r,"mac")));if(mac.empty()||(*c.mac&&mac!=c.mac))ok=false;
    on=cJSON_IsTrue(state);name.clear();auto encoded=text(get(r,"nickname"));
    if(encoded.size()<192){unsigned char decoded[145]{};size_t length=0;if(mbedtls_base64_decode(decoded,sizeof(decoded)-1,&length,reinterpret_cast<const unsigned char *>(encoded.data()),encoded.size())==0&&length<=96)name.assign(reinterpret_cast<char *>(decoded),length);}
    cJSON_Delete(body);return ok;
}
void run(void *){
    Transport transport;TapoClient client;client.set_transport(&transport);
    circulation::Policy policy;uint32_t revision=UINT32_MAX;uint64_t next_read=0,next_discovery=0,next_clock=0;unsigned failures=0;
    auto find=[&](const Config &c){
        auto found=tapo_discovery::scan();{Lock l(mutex);discovered=found;scan_busy=false;++scan_sequence;}
        if(!*c.mac||!*c.identity)return;
        for(const auto &device:found)if(device.mac==c.mac&&device.host!=c.host){
            Lock save(config_mutex);{Lock l(mutex);if(c.revision!=config.revision)return;}
            Config updated=c;snprintf(updated.host,sizeof(updated.host),"%s",device.host.c_str());++updated.revision;
            updates::FlashGuard flash;if(flash.acquired&&store(updated)){Lock l(mutex);config=updated;status="Odnaleziono P110 po MAC";}return;
        }
    };
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(500));Config c;{Lock l(mutex);c=config;}
        bool scan;{Lock l(mutex);scan=scan_requested;scan_requested=false;if(scan)scan_busy=true;}
        if(scan&&state::network().connected&&!updates::installing()){find(c);next_discovery=now()+60000;}
        else if(scan){Lock l(mutex);scan_busy=false;++scan_sequence;}
        if(!c.credentials||!state::network().connected||updates::installing()){
            client.reset();{Lock l(mutex);linked=false;if(c.credentials)status=updates::installing()?"Aktualizacja ESP":"Brak Wi-Fi ESP";}
            continue;
        }
        if(c.revision!=revision){revision=c.revision;next_read=0;failures=0;policy.demand=false;client.reset();transport.host=c.host;Lock l(mutex);pending=false;}
        double temperature;uint64_t age;bool sensor=boiler::water_heater_temperature(temperature,age);
        auto seconds=time(nullptr);tm local{};localtime_r(&seconds,&local);
        circulation::Input input;input.armed=c.armed&&*c.identity;input.fresh=sensor;input.temperature=temperature;
        input.clock=seconds>=1735689600&&seconds<=4102444800;input.day=(local.tm_wday+6)%7;input.slot=local.tm_hour*2+local.tm_min/30;input.now_ms=now();
        bool current_known,current_on,waiting;{Lock l(mutex);input.last_off_ms=last_off_ms;current_known=known&&linked;current_on=actual_on;waiting=pending;}
        auto d=policy.step(c.rules,input);{Lock l(mutex);decision=d;}
        bool need_change=d.manage&&current_known&&current_on!=d.on;
        if(now()<next_read&&(!need_change||failures||waiting))continue;
        if(!client.is_ready()&&!client.handshake({c.auth,c.auth+32})){
            if(now()>=next_discovery){find(c);next_discovery=now()+60000;}
            Lock l(mutex);linked=false;status="Brak logowania Tapo: sprawdz dane i zgodnosc aplikacji innych firm";
            next_read=now()+std::min(60000U,5000U<<std::min(failures++,3U));continue;
        }
        std::string found,name,type,mac;bool on=false;
        if(!read(client,c,found,on,name,type,mac)){
            if(now()>=next_discovery){find(c);next_discovery=now()+60000;}
            client.reset();Lock l(mutex);linked=false;status="Brak potwierdzonego odczytu P110 lub inne urzadzenie pod tym IP";
            next_read=now()+std::min(60000U,5000U<<std::min(failures++,3U));continue;
        }
        {Lock l(mutex);if(c.revision!=config.revision)continue;if(!on&&(!known||actual_on))last_off_ms=now();linked=known=true;actual_on=on;verified_ms=now();alias=name;model=type;status="Polaczono lokalnie";input.last_off_ms=last_off_ms;}
        if(!input.clock&&now()>=next_clock){
            cJSON *clock=nullptr;if(call(client,R"JSON({"method":"get_device_time"})JSON",clock)){
                auto *value=get(get(clock,"result"),"timestamp");if(cJSON_IsNumber(value)&&value->valuedouble>=1735689600&&value->valuedouble<=4102444800)panel::set_clock(value->valuedouble);
            }
            cJSON_Delete(clock);next_clock=now()+300000;
        }
        input.now_ms=now();input.fresh=boiler::water_heater_temperature(input.temperature,age);
        seconds=time(nullptr);localtime_r(&seconds,&local);input.clock=seconds>=1735689600&&seconds<=4102444800;
        input.day=(local.tm_wday+6)%7;input.slot=local.tm_hour*2+local.tm_min/30;
        d=policy.step(c.rules,input);{Lock l(mutex);decision=d;}
        failures=0;
        if(!*c.identity){
            Lock save(config_mutex);{Lock l(mutex);if(c.revision!=config.revision)continue;}
            snprintf(c.identity,sizeof(c.identity),"%s",found.c_str());
            snprintf(c.mac,sizeof(c.mac),"%s",mac.c_str());
            updates::FlashGuard flash;if(flash.acquired&&store(c)){Lock l(mutex);config=c;}else{Lock l(mutex);status="Nie zapisano powiazania P110";}
            next_read=now()+10000;continue;
        }
        if(d.manage&&on!=d.on){
            {Lock l(mutex);if(c.revision!=config.revision)continue;pending=true;status="Oczekiwanie na potwierdzenie pompki";}
            cJSON *response=nullptr;
            bool acknowledged=call(client,d.on?R"JSON({"method":"set_device_info","params":{"device_on":true}})JSON":R"JSON({"method":"set_device_info","params":{"device_on":false}})JSON",response);
            cJSON_Delete(response);{Lock l(mutex);++writes;}
            bool observed=false;std::string check_id,check_name,check_type,check_mac;
            bool checked=read(client,c,check_id,observed,check_name,check_type,check_mac);
            {Lock l(mutex);linked=checked;if(checked){known=true;actual_on=observed;verified_ms=now();if(!observed)last_off_ms=now();}
             pending=!checked||observed!=d.on;status=checked&&observed==d.on?"Stan potwierdzony w P110":"Zmiana niepotwierdzona; oczekiwanie na odczyt";}
            if(!acknowledged||!checked)client.reset();
        }else{Lock l(mutex);pending=false;}
        next_read=now()+10000;
    }
}
bool start_worker(){return worker||xTaskCreate(run,"cwu_tapo",6144,nullptr,2,&worker)==pdPASS;}
const char *reason(circulation::Reason r){
    const char *names[]={"Sterowanie nieaktywne","Nieprawidlowe progi","Wylaczone","Brak aktualnej temperatury CWU","Brak czasu do harmonogramu","Poza godzinami harmonogramu","CWU ponizej dolnego progu","Oczekiwanie na gorny prog","Przerwa przed ponownym wlaczeniem","Wlaczenie reczne","CWU powyzej progu wlaczenia"};
    return names[static_cast<unsigned>(r)];
}
}
void start(){
    mutex=xSemaphoreCreateMutex();config_mutex=xSemaphoreCreateMutex();configASSERT(mutex&&config_mutex);
    nvs_handle_t h;if(nvs_open("cwu_pump",NVS_READONLY,&h)==ESP_OK){Config saved;size_t size=sizeof(saved);auto e=nvs_get_blob(h,"config_v1",&saved,&size);nvs_close(h);
        saved.host[15]=0;saved.identity[64]=0;saved.mac[12]=0;
        for(auto &id:saved.requests)id[32]=0;
        if(e==ESP_OK&&size==sizeof(saved)&&saved.magic==config.magic&&saved.request_cursor<8&&circulation::valid(saved.rules)&&(!saved.credentials||private_ip(saved.host)))config=saved;}
    setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();
    if(config.credentials&&!start_worker())status="Brak pamieci na sterowanie Tapo";
}
bool self_test(){
    std::vector<uint8_t> local(16),remote(16);for(unsigned i=0;i<16;i++){local[i]=i;remote[i]=16+i;}
    auto hex=[](const char *s){std::vector<uint8_t> v;auto n=[](char c){return c>='0'&&c<='9'?c-'0':c-'a'+10;};for(size_t i=0;s[i]&&s[i+1];i+=2)v.push_back((n(s[i])<<4)|n(s[i+1]));return v;};
    auto auth=hex("7c548591a7d9a37d2684666a0b8127c50b72d73d96098be2746ebeb928bd0e21");
    auto expected=hex("548539b9e0962f50a9bad9cb860f1afbd0d7fcd66c9bf06da9ee80c8dcc50cf58c9d41df8510a7d400547f5cb7690ff2d9d2b6498bd7c901ad087a0d6f204ce5");
    TapoCipher cipher(local,remote,auth);auto encrypted=cipher.encrypt("tapo-public-fixture");
    if(encrypted.seq!=118336000||encrypted.body!=expected)return false;
    std::string plain;if(!cipher.decrypt(encrypted.seq,encrypted.body,plain)||plain!="tapo-public-fixture")return false;
    encrypted.body[0]^=1;if(cipher.decrypt(encrypted.seq,encrypted.body,plain))return false;
    circulation::Rules rules;rules.mode=circulation::Mode::automatic;circulation::Input input;
    input.armed=input.fresh=true;input.now_ms=30000;input.temperature=45;circulation::Policy p;
    if(!p.step(rules,input).on)return false;
    input.fresh=false;return !p.step(rules,input).on;
}
bool configure(const cJSON *request,std::string &error){
    Lock save(config_mutex);Config next;{Lock l(mutex);next=config;}
    auto action=text(get(request,"action"));
    auto request_id=text(get(request,"request_id"));
    if(request_id.size()!=32||request_id.find_first_not_of("0123456789abcdef")!=std::string::npos){error="Brak identyfikatora operacji";return false;}
    for(const auto &seen:next.requests)if(request_id==seen)return true;
    if(action=="scan"){
        if(!start_worker()){error="Brak pamieci na wyszukiwanie";return false;}
        Lock l(mutex);if(request_id==scan_request_id)return true;if(scan_busy||scan_requested){error="Wyszukiwanie juz trwa";return false;}scan_request_id=request_id;scan_requested=true;return true;
    }
    if(action=="connect"){
        auto host=text(get(request,"host")),user=text(get(request,"username")),password=text(get(request,"password"));
        if(!private_ip(host.c_str())||user.empty()||user.size()>128||password.empty()||password.size()>128){error="Sprawdz IP oraz dane konta Tapo";return false;}
        {Lock l(mutex);if(config.armed&&(!known||actual_on)){error="Najpierw wylacz pompke i poczekaj na potwierdzenie";return false;}}
        uint8_t user_hash[20]{},pass_hash[20]{};
        bool hashed=mbedtls_sha1(reinterpret_cast<const unsigned char *>(user.data()),user.size(),user_hash)==0&&
            mbedtls_sha1(reinterpret_cast<const unsigned char *>(password.data()),password.size(),pass_hash)==0;
        std::vector<uint8_t> input(user_hash,user_hash+20);input.insert(input.end(),pass_hash,pass_hash+20);
        auto auth=hashed?TapoCipher::sha256(input):std::vector<uint8_t>{};
        mbedtls_platform_zeroize(user_hash,sizeof(user_hash));mbedtls_platform_zeroize(pass_hash,sizeof(pass_hash));
        mbedtls_platform_zeroize(user.data(),user.size());mbedtls_platform_zeroize(password.data(),password.size());
        if(auth.size()!=32){error="Nie przygotowano logowania";return false;}
        auto mac=tapo_discovery::normalize_mac(text(get(request,"mac")));
        if(!text(get(request,"mac")).empty()&&mac.empty()){error="Nieprawidlowy MAC P110";return false;}
        memcpy(next.auth,auth.data(),32);next.credentials=true;next.armed=false;next.identity[0]=0;snprintf(next.host,sizeof(next.host),"%s",host.c_str());snprintf(next.mac,sizeof(next.mac),"%s",mac.c_str());
    }else if(action=="settings"){
        auto *low=get(request,"lower"),*high=get(request,"upper"),*mode=get(request,"mode"),*schedule=get(request,"schedule_enabled"),*week=get(request,"week");
        if(!cJSON_IsNumber(low)||!cJSON_IsNumber(high)||!cJSON_IsNumber(mode)||!cJSON_IsBool(schedule)||!cJSON_IsArray(week)||cJSON_GetArraySize(week)!=7){error="Nieprawidlowe ustawienia pompki";return false;}
        if(!std::isfinite(low->valuedouble)||!std::isfinite(high->valuedouble)||low->valuedouble<5||high->valuedouble>85||high->valuedouble-low->valuedouble<.5||mode->valuedouble!=mode->valueint||mode->valueint<0||mode->valueint>2){error="Dolny prog 5-84,5 C; gorny do 85 C; roznica co najmniej 0,5 C";return false;}
        next.rules.low=std::lround(low->valuedouble*10);next.rules.high=std::lround(high->valuedouble*10);next.rules.mode=static_cast<circulation::Mode>(mode->valueint);next.rules.schedule=cJSON_IsTrue(schedule);
        for(unsigned day=0;day<7;day++){auto *row=cJSON_GetArrayItem(week,day);if(!cJSON_IsArray(row)||cJSON_GetArraySize(row)!=6){error="Nieprawidlowy tydzien";return false;}for(unsigned byte=0;byte<6;byte++){auto *v=cJSON_GetArrayItem(row,byte);if(!cJSON_IsNumber(v)||v->valuedouble!=v->valueint||v->valueint<0||v->valueint>255){error="Nieprawidlowy tydzien";return false;}next.rules.week[day][byte]=v->valueint;}}
        if(!next.credentials||!*next.identity){error="Najpierw polacz i potwierdz P110";return false;}
        next.armed=true;
    }else{error="Nieznane polecenie pompki";return false;}
    if(!circulation::valid(next.rules)){error="Nieprawidlowe progi";return false;}
    snprintf(next.requests[next.request_cursor],33,"%s",request_id.c_str());next.request_cursor=(next.request_cursor+1)%8;
    next.revision++;
    updates::FlashGuard flash;if(!flash.acquired||!store(next)){error="Nie zapisano konfiguracji; trwa aktualizacja lub blad pamieci";return false;}
    {Lock l(mutex);config=next;if(action=="connect"){known=linked=false;alias.clear();model.clear();}status="Oczekiwanie na odczyt P110";}
    if(!start_worker()){error="Brak pamieci na zadanie Tapo";return false;}return true;
}
cJSON *snapshot(){
    auto *out=cJSON_CreateObject();if(!out)return nullptr;
    double temp;uint64_t age;bool sensor=boiler::water_heater_temperature(temp,age);
    Lock l(mutex);bool fresh=linked&&known&&now()-verified_ms<=20000;
    cJSON_AddBoolToObject(out,"configured",config.credentials);cJSON_AddBoolToObject(out,"paired",*config.identity);cJSON_AddBoolToObject(out,"armed",config.armed);
    cJSON_AddStringToObject(out,"host",config.host);cJSON_AddStringToObject(out,"alias",alias.c_str());cJSON_AddStringToObject(out,"model",model.c_str());
    cJSON_AddStringToObject(out,"mac",config.mac);cJSON_AddBoolToObject(out,"scan_busy",scan_busy||scan_requested);cJSON_AddNumberToObject(out,"scan_sequence",scan_sequence);
    auto *devices=cJSON_AddArrayToObject(out,"devices");for(const auto &device:discovered){auto *v=cJSON_CreateObject();cJSON_AddStringToObject(v,"host",device.host.c_str());cJSON_AddStringToObject(v,"mac",device.mac.c_str());cJSON_AddStringToObject(v,"model",device.model.c_str());cJSON_AddStringToObject(v,"protocol",device.protocol.c_str());cJSON_AddItemToArray(devices,v);}
    cJSON_AddStringToObject(out,"status",status.c_str());cJSON_AddBoolToObject(out,"fresh",fresh);cJSON_AddBoolToObject(out,"pending",pending);
    if(fresh)cJSON_AddBoolToObject(out,"on",actual_on);else cJSON_AddNullToObject(out,"on");
    if(sensor)cJSON_AddNumberToObject(out,"temperature",temp);else cJSON_AddNullToObject(out,"temperature");
    cJSON_AddBoolToObject(out,"sensor_fresh",sensor);cJSON_AddStringToObject(out,"reason",reason(decision.reason));
    cJSON_AddNumberToObject(out,"lower",config.rules.low/10.0);cJSON_AddNumberToObject(out,"upper",config.rules.high/10.0);
    cJSON_AddNumberToObject(out,"mode",static_cast<unsigned>(config.rules.mode));cJSON_AddBoolToObject(out,"schedule_enabled",config.rules.schedule);
    cJSON_AddNumberToObject(out,"revision",config.revision);cJSON_AddNumberToObject(out,"write_count",writes);
    cJSON_AddBoolToObject(out,"clock_valid",time(nullptr)>=1735689600);
    auto *week=cJSON_AddArrayToObject(out,"week");for(auto &day:config.rules.week){auto *row=cJSON_CreateArray();for(auto byte:day)cJSON_AddItemToArray(row,cJSON_CreateNumber(byte));cJSON_AddItemToArray(week,row);}
    return out;
}
}
