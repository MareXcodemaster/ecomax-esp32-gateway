#include "cwu_pump.hpp"
#include "circulation_policy.hpp"
#include "tapo_discovery.hpp"
#include "tapo_energy.hpp"
#include "third_party/esp_tapo/tapo_client.h"
#include "smart_tapo.hpp"
#include "esp_random.h"
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
using TapoClient=smart_tapo::Client;
using esphome::tapo::TapoCipher;
using TapoTransport=smart_tapo::Transport;
struct LegacyConfig {
    uint32_t magic=0x43575031,revision=0;
    circulation::Rules rules;
    bool credentials=false,armed=false;
    char host[16]{},identity[65]{},mac[13]{};
    uint8_t auth[32]{};
    char requests[8][33]{};
    uint8_t request_cursor=0;
};
struct ConfigV2:LegacyConfig {
    uint8_t shadow[20]{};
    bool has_shadow=false,has_klap=false,auth_blocked=false;
};
struct Config:ConfigV2 {char auth_error[128]{},auth_stage[24]{};int auth_http=0,auth_code=0;uint32_t derive_ms=0,auth_ms=0;bool enabled=false;};
Config config;
SemaphoreHandle_t mutex,config_mutex;
TaskHandle_t worker=nullptr;
bool known=false,actual_on=false,linked=false,pending=false;
uint64_t verified_ms=0,last_off_ms=0;
unsigned writes=0;
tapo_energy::Usage energy;
tapo_energy::Days energy_days;
tapo_energy::Days energy_staging; // Single worker scratch, outside its bounded task stack.
char energy_request[180]{};
uint64_t energy_ms=0,energy_history_ms=0;
time_t energy_epoch=0;
std::string status="Nie skonfigurowano",alias,model,protocol;
circulation::Decision decision;
std::vector<tapo_discovery::Device> discovered;
bool scan_requested=false,scan_busy=false;
unsigned scan_sequence=0;
std::string scan_request_id;
struct Lock {SemaphoreHandle_t m;Lock(SemaphoreHandle_t v):m(v){xSemaphoreTake(m,portMAX_DELAY);}~Lock(){xSemaphoreGive(m);}};
uint64_t now(){return esp_timer_get_time()/1000;}
int random_fill(void *,unsigned char *out,size_t size){esp_fill_random(out,size);return 0;}
void crypto_yield(){vTaskDelay(1);}
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
    auto e=nvs_set_blob(h,"config_v3",&c,sizeof(c));if(e==ESP_OK)e=nvs_commit(h);nvs_close(h);return e==ESP_OK;
}
class Transport:public TapoTransport {
    esp_http_client_handle_t client_=nullptr;
    std::string connected_host_;
    Response *active_=nullptr;
    bool server_close_=false;
public:
    std::string host;
    ~Transport(){reset();}
    void reset()override{if(client_){active_=nullptr;esp_http_client_close(client_);esp_http_client_cleanup(client_);client_=nullptr;}connected_host_.clear();}
    Response post(const std::string &path,const std::string &body,const std::string &cookie)override{
        if(path.rfind("/app/",0)!=0)return {};
        return http(path,body,"application/octet-stream",cookie);
    }
    Response exchange(const std::string &path,const std::string &body,const char *type)override{
        bool session=path.rfind("/stok=",0)==0&&path.size()>9&&path.size()<150&&path.substr(path.size()-3)=="/ds"&&
            path.substr(6,path.size()-9).find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-")==std::string::npos;
        if(path!="/"&&!session)return {};
        return http(path,body,type,{});
    }
    Response http(const std::string &path,const std::string &body,const char *type,const std::string &cookie){
        Response out;if(!private_ip(host.c_str())||body.size()>8192)return out;
        std::string url="http://"+host+path;
        if(client_&&connected_host_!=host)reset();
        esp_http_client_config_t cfg{};cfg.url=url.c_str();cfg.timeout_ms=2500;cfg.disable_auto_redirect=true;cfg.buffer_size=512;cfg.keep_alive_enable=true;
        cfg.event_handler=[](esp_http_client_event_t *event)->esp_err_t{
            auto *self=static_cast<Transport *>(event->user_data);auto *response=self->active_;if(!response)return ESP_OK;
            if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&!strcasecmp(event->header_key,"Connection")&&!strcasecmp(event->header_value,"close"))self->server_close_=true;
            if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&!strcasecmp(event->header_key,"Set-Cookie")){
                if(response->set_cookie.size()+strlen(event->header_value)+2<384){if(!response->set_cookie.empty())response->set_cookie+="; ";response->set_cookie+=event->header_value;}
            }
            return ESP_OK;
        };
        cfg.user_data=this;if(!client_){client_=esp_http_client_init(&cfg);if(!client_)return out;connected_host_=host;}
        auto client=client_;active_=&out;server_close_=false;
        if(esp_http_client_set_url(client,url.c_str())!=ESP_OK){reset();return out;}
        esp_http_client_set_method(client,HTTP_METHOD_POST);esp_http_client_set_header(client,"Content-Type",type);
        esp_http_client_set_header(client,"Connection","keep-alive");
        if(!cookie.empty())esp_http_client_set_header(client,"Cookie",cookie.c_str());else esp_http_client_delete_header(client,"Cookie");
        auto deadline=esp_timer_get_time()+5000000;
        if(esp_http_client_open(client,body.size())==ESP_OK){
            int sent=0;while(sent<static_cast<int>(body.size())){int n=esp_http_client_write(client,body.data()+sent,body.size()-sent);if(n<=0)break;sent+=n;}
            if(sent==static_cast<int>(body.size())&&esp_http_client_fetch_headers(client)>=0){
                out.status=esp_http_client_get_status_code(client);char buffer[512];
                while(out.body.size()<8192&&esp_timer_get_time()<deadline){int n=esp_http_client_read(client,buffer,std::min(sizeof(buffer),8192-out.body.size()));if(n<0){out.status=-1;break;}if(!n)break;out.body.append(buffer,n);}
                if(esp_timer_get_time()>=deadline)out.status=-1;
                if(out.body.size()>=8192)out.status=-1;
                if(!esp_http_client_is_complete_data_received(client))out.status=-1;
            }
        }
        active_=nullptr;if(out.status<=0||server_close_)reset();return out;
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
    ok=ok&&tapo_discovery::supported_model(type)&&!id.empty()&&id.size()<160&&cJSON_IsBool(state);
    found_id=identity(id);if(*c.identity&&found_id!=c.identity)ok=false;
    mac=tapo_discovery::normalize_mac(text(get(r,"mac")));if(mac.empty()||(*c.mac&&mac!=c.mac))ok=false;
    on=cJSON_IsTrue(state);name.clear();auto encoded=text(get(r,"nickname"));
    if(encoded.size()<192){unsigned char decoded[145]{};size_t length=0;if(mbedtls_base64_decode(decoded,sizeof(decoded)-1,&length,reinterpret_cast<const unsigned char *>(encoded.data()),encoded.size())==0&&length<=96)name.assign(reinterpret_cast<char *>(decoded),length);}
    cJSON_Delete(body);return ok;
}
void run(void *){
    Transport transport;TapoClient client(random_fill,nullptr,crypto_yield,now);client.set_transport(&transport);
    circulation::Policy policy;uint32_t revision=UINT32_MAX;uint64_t next_read=0,next_discovery=0,next_clock=0,next_energy=0,next_history=0;unsigned failures=0;
    std::string policy_identity;
    auto find=[&](const Config &c){
        auto found=tapo_discovery::scan();{Lock l(mutex);discovered=found;scan_busy=false;++scan_sequence;}
        if(!*c.mac||!*c.identity)return;
        for(const auto &device:found)if(device.mac==c.mac&&device.host!=c.host){
            Lock save(config_mutex);{Lock l(mutex);if(c.revision!=config.revision)return;}
            Config updated=c;snprintf(updated.host,sizeof(updated.host),"%s",device.host.c_str());++updated.revision;
            updates::FlashGuard flash;if(flash.acquired&&store(updated)){Lock l(mutex);config=updated;status="Odnaleziono Tapo po MAC";}return;
        }
    };
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(500));Config c;{Lock l(mutex);c=config;}
        if(!c.enabled){client.reset();policy.demand=false;Lock l(mutex);known=linked=pending=scan_requested=scan_busy=false;decision={};status="Obsluga cyrkulacji wylaczona";continue;}
        bool scan;{Lock l(mutex);scan=scan_requested;scan_requested=false;if(scan)scan_busy=true;}
        if(scan&&state::network().connected&&!updates::installing()){find(c);next_discovery=now()+60000;}
        else if(scan){Lock l(mutex);scan_busy=false;++scan_sequence;}
        if(!c.credentials||!state::network().connected||updates::installing()){
            client.reset();{Lock l(mutex);linked=false;if(c.credentials)status=updates::installing()?"Aktualizacja ESP":"Brak Wi-Fi ESP";}
            continue;
        }
        if(c.auth_blocked){client.reset();Lock l(mutex);linked=false;status=*c.auth_error?c.auth_error:"Logowanie Tapo wstrzymane. Wpisz poprawne haslo i polacz ponownie";continue;}
        if(c.revision!=revision){revision=c.revision;next_read=0;failures=0;if(policy_identity!=c.identity){policy.demand=false;next_energy=next_history=0;Lock l(mutex);energy={};energy_days={};energy_epoch=0;energy_ms=energy_history_ms=0;}policy_identity=c.identity;client.reset();transport.host=c.host;Lock l(mutex);pending=false;}
        double temperature;uint64_t age;bool sensor=boiler::water_heater_temperature(temperature,age);
        auto seconds=time(nullptr);tm local{};localtime_r(&seconds,&local);
        circulation::Input input;input.armed=c.armed&&*c.identity;input.fresh=sensor;input.temperature=temperature;
        input.clock=seconds>=1735689600&&seconds<=4102444800;input.day=(local.tm_wday+6)%7;input.slot=local.tm_hour*2+local.tm_min/30;input.now_ms=now();
        bool current_known,current_on,waiting;{Lock l(mutex);input.last_off_ms=last_off_ms;current_known=known&&linked;current_on=actual_on;waiting=pending;}
        auto d=policy.step(c.rules,input);{Lock l(mutex);decision=d;}
        bool need_change=d.manage&&current_known&&current_on!=d.on;
        if(now()<next_read&&(!need_change||failures||waiting))continue;
        if(!client.is_ready()&&!client.handshake({c.auth,c.auth+32},c.has_shadow?std::vector<uint8_t>(c.shadow,c.shadow+20):std::vector<uint8_t>{},c.has_klap)){
            if(client.credentials_rejected()){
                Lock save(config_mutex);{Lock l(mutex);if(c.revision!=config.revision)continue;}
                Config blocked=c;blocked.auth_blocked=true;
                snprintf(blocked.auth_error,sizeof(blocked.auth_error),"%s",client.error().c_str());snprintf(blocked.auth_stage,sizeof(blocked.auth_stage),"%s",client.trace().stage.c_str());
                blocked.auth_http=client.trace().http;blocked.auth_code=client.trace().code;blocked.derive_ms=client.trace().derive_ms;blocked.auth_ms=client.trace().total_ms;
                state::log("Tapo auth stopped: stage=%s http=%d code=%d derive_ms=%lu total_ms=%lu",blocked.auth_stage,blocked.auth_http,blocked.auth_code,(unsigned long)blocked.derive_ms,(unsigned long)blocked.auth_ms);
                updates::FlashGuard flash;
                bool persisted=flash.acquired&&store(blocked);{Lock l(mutex);config.auth_blocked=true;linked=false;protocol=client.protocol();status=persisted?client.error():"Logowanie wstrzymane; nie zapisano blokady w pamieci";}
                {Lock l(mutex);if(config.revision==blocked.revision)config=blocked;}
                continue;
            }
            if(now()>=next_discovery){find(c);next_discovery=now()+60000;}
            Lock l(mutex);linked=false;protocol=client.protocol();status=client.error();
            next_read=now()+std::min(60000U,5000U<<std::min(failures++,3U));continue;
        }
        std::string found,name,type,mac;bool on=false;
        if(!read(client,c,found,on,name,type,mac)){
            if(now()>=next_discovery){find(c);next_discovery=now()+60000;}
            client.reset();Lock l(mutex);linked=false;status=client.error().empty()?"Brak potwierdzonego odczytu Tapo lub inne urzadzenie pod tym IP":client.error();
            next_read=now()+std::min(60000U,5000U<<std::min(failures++,3U));continue;
        }
        {Lock l(mutex);if(c.revision!=config.revision)continue;if(!on&&(!known||actual_on))last_off_ms=now();linked=known=true;actual_on=on;verified_ms=now();alias=name;model=type;protocol=client.protocol();status="Polaczono lokalnie / "+protocol;input.last_off_ms=last_off_ms;}
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
            updates::FlashGuard flash;if(flash.acquired&&store(c)){Lock l(mutex);config=c;}else{Lock l(mutex);status="Nie zapisano powiazania Tapo";}
            next_read=now()+10000;continue;
        }
        bool changing=d.manage&&on!=d.on;
        if(changing){
            {Lock l(mutex);if(c.revision!=config.revision)continue;pending=true;status="Oczekiwanie na potwierdzenie pompki";}
            cJSON *response=nullptr;
            bool acknowledged=call(client,d.on?R"JSON({"method":"set_device_info","params":{"device_on":true}})JSON":R"JSON({"method":"set_device_info","params":{"device_on":false}})JSON",response);
            cJSON_Delete(response);{Lock l(mutex);++writes;}
            bool observed=false;std::string check_id,check_name,check_type,check_mac;
            bool checked=read(client,c,check_id,observed,check_name,check_type,check_mac);
            {Lock l(mutex);linked=checked;if(checked){known=true;actual_on=observed;verified_ms=now();if(!observed)last_off_ms=now();}
             pending=!checked||observed!=d.on;status=checked&&observed==d.on?"Stan potwierdzony w Tapo":"Zmiana niepotwierdzona; oczekiwanie na odczyt";}
            if(!acknowledged||!checked)client.reset();
        }else{Lock l(mutex);pending=false;}
        // One bounded optional telemetry request per cycle, after relay control.
        bool metered=tapo_discovery::metered_model(type);
        bool current;{Lock l(mutex);current=c.revision==config.revision;}
        if(!changing&&metered&&current&&client.is_ready()&&!updates::installing()){
            if(now()>=next_energy){
                cJSON *body=nullptr;bool ok=call(client,R"JSON({"method":"get_energy_usage"})JSON",body);
                if(ok){auto value=tapo_energy::usage(get(body,"result"));Lock l(mutex);if(c.revision==config.revision){energy=value;energy_ms=now();energy_epoch=time(nullptr);}}
                cJSON_Delete(body);next_energy=now()+30000;
            }else if(now()>=next_history){
                auto &days=energy_staging;days={};
                if(tapo_energy::range(time(nullptr),days)){
                    snprintf(energy_request,sizeof(energy_request),"{\"method\":\"get_energy_data\",\"params\":{\"interval\":60,\"start_timestamp\":%lld,\"end_timestamp\":%lld}}",static_cast<long long>(days.start[0]),static_cast<long long>(days.start[7]-1));
                    cJSON *body=nullptr;bool ok=call(client,energy_request,body)&&tapo_energy::hourly(get(body,"result"),days);
                    if(ok){Lock l(mutex);if(c.revision==config.revision){energy_days=days;energy_history_ms=now();}}
                    cJSON_Delete(body);
                }
                next_history=now()+600000;
            }
        }
        next_read=now()+10000;
    }
}
bool start_worker(){return worker||xTaskCreate(run,"cwu_tapo",6144,nullptr,2,&worker)==pdPASS;}
const char *reason(circulation::Reason r){
    const char *names[]={"Sterowanie nieaktywne","Nieprawidlowe progi","Wylaczone","Brak aktualnej temperatury CWU","Brak czasu do harmonogramu","Poza godzinami harmonogramu","CWU ponizej dolnego progu","Oczekiwanie na gorny prog","Przerwa przed ponownym wlaczeniem","Wlaczenie reczne","CWU powyzej progu wlaczenia","Recznie: pompa wylaczona"};
    return names[static_cast<unsigned>(r)];
}
}
void start(){
    mutex=xSemaphoreCreateMutex();config_mutex=xSemaphoreCreateMutex();configASSERT(mutex&&config_mutex);
    bool migrate=false;
    nvs_handle_t h;if(nvs_open("cwu_pump",NVS_READONLY,&h)==ESP_OK){Config saved;size_t size=sizeof(saved);auto e=nvs_get_blob(h,"config_v3",&saved,&size);
        if(e==ESP_ERR_NVS_NOT_FOUND){ConfigV2 v2;size_t n=sizeof(v2);e=nvs_get_blob(h,"config_v2",&v2,&n);if(e==ESP_OK&&n==sizeof(v2)){static_cast<ConfigV2&>(saved)=v2;size=sizeof(saved);migrate=true;}else if(e!=ESP_ERR_NVS_NOT_FOUND)e=ESP_FAIL;}
        if(e==ESP_ERR_NVS_NOT_FOUND){LegacyConfig legacy;size_t old_size=sizeof(legacy);e=nvs_get_blob(h,"config_v1",&legacy,&old_size);if(e==ESP_OK&&old_size==sizeof(legacy)){static_cast<LegacyConfig&>(saved)=legacy;saved.has_klap=legacy.credentials;size=sizeof(saved);migrate=true;}else e=ESP_FAIL;}nvs_close(h);
        saved.host[15]=0;saved.identity[64]=0;saved.mac[12]=0;
        saved.auth_error[127]=0;saved.auth_stage[23]=0;
        for(auto &id:saved.requests)id[32]=0;
        if(e==ESP_OK&&size==sizeof(saved)&&saved.magic==config.magic&&saved.request_cursor<8&&circulation::valid(saved.rules)&&(!saved.credentials||private_ip(saved.host)))config=saved;}
    setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();
    if(migrate){config.enabled=config.credentials;Config next=config;++next.revision;updates::FlashGuard flash;if(flash.acquired&&store(next)){config=next;state::log("Tapo migration: saved credentials, control and authentication stop preserved");}}
    if(config.enabled&&config.credentials&&!start_worker())status="Brak pamieci na sterowanie Tapo";
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
    input.fresh=false;return !p.step(rules,input).on&&tapo_tpap::self_test(random_fill,nullptr,crypto_yield);
}
bool configure(const cJSON *request,std::string &error){
    Lock save(config_mutex);Config next;{Lock l(mutex);next=config;}
    auto action=text(get(request,"action"));
    auto request_id=text(get(request,"request_id"));
    if(request_id.size()!=32||request_id.find_first_not_of("0123456789abcdef")!=std::string::npos){error="Brak identyfikatora operacji";return false;}
    for(const auto &seen:next.requests)if(request_id==seen)return true;
    if(action!="feature"&&!next.enabled){error="Najpierw wlacz obsluge cyrkulacji CWU";return false;}
    if(action=="scan"){
        if(!start_worker()){error="Brak pamieci na wyszukiwanie";return false;}
        Lock l(mutex);if(request_id==scan_request_id)return true;if(scan_busy||scan_requested){error="Wyszukiwanie juz trwa";return false;}scan_request_id=request_id;scan_requested=true;return true;
    }
    if(action=="feature"){
        auto *enabled=get(request,"enabled");if(!cJSON_IsBool(enabled)){error="Nieprawidlowy stan funkcji";return false;}
        next.enabled=cJSON_IsTrue(enabled);
    }else if(action=="connect"){
        auto host=text(get(request,"host")),user=text(get(request,"username")),password=text(get(request,"password"));
        if(!private_ip(host.c_str())||user.size()>128||password.empty()||password.size()>128){error="Sprawdz IP oraz haslo konta Tapo";return false;}
        {Lock l(mutex);if(config.armed&&(!known||!linked||now()-verified_ms>20000||actual_on)){error="Najpierw wylacz pompke i poczekaj na potwierdzenie";return false;}}
        uint8_t user_hash[20]{},pass_hash[20]{};
        bool hashed=mbedtls_sha1(reinterpret_cast<const unsigned char *>(user.data()),user.size(),user_hash)==0&&
            mbedtls_sha1(reinterpret_cast<const unsigned char *>(password.data()),password.size(),pass_hash)==0;
        std::vector<uint8_t> input(user_hash,user_hash+20);input.insert(input.end(),pass_hash,pass_hash+20);
        auto auth=hashed?TapoCipher::sha256(input):std::vector<uint8_t>{};
        if(hashed){memcpy(next.shadow,pass_hash,20);next.has_shadow=true;next.has_klap=!user.empty();next.auth_blocked=false;next.auth_error[0]=0;next.auth_stage[0]=0;next.auth_code=next.auth_http=0;next.derive_ms=next.auth_ms=0;}
        mbedtls_platform_zeroize(user_hash,sizeof(user_hash));mbedtls_platform_zeroize(pass_hash,sizeof(pass_hash));
        mbedtls_platform_zeroize(user.data(),user.size());mbedtls_platform_zeroize(password.data(),password.size());
        if(auth.size()!=32){error="Nie przygotowano logowania";return false;}
        auto mac=tapo_discovery::normalize_mac(text(get(request,"mac")));
        if(!text(get(request,"mac")).empty()&&mac.empty()){error="Nieprawidlowy MAC Tapo";return false;}
        memcpy(next.auth,auth.data(),32);next.credentials=true;next.armed=false;next.identity[0]=0;snprintf(next.host,sizeof(next.host),"%s",host.c_str());snprintf(next.mac,sizeof(next.mac),"%s",mac.c_str());
    }else if(action=="settings"){
        auto *low=get(request,"lower"),*high=get(request,"upper"),*mode=get(request,"mode"),*schedule=get(request,"schedule_enabled"),*week=get(request,"week");
        if(!cJSON_IsNumber(low)||!cJSON_IsNumber(high)||!cJSON_IsNumber(mode)||!cJSON_IsBool(schedule)||!cJSON_IsArray(week)||cJSON_GetArraySize(week)!=7){error="Nieprawidlowe ustawienia pompki";return false;}
        if(!std::isfinite(low->valuedouble)||!std::isfinite(high->valuedouble)||low->valuedouble<5||high->valuedouble>85||high->valuedouble-low->valuedouble<.5||mode->valuedouble!=mode->valueint||mode->valueint<0||mode->valueint>3){error="Dolny prog 5-84,5 C; gorny do 85 C; roznica co najmniej 0,5 C";return false;}
        next.rules.low=std::lround(low->valuedouble*10);next.rules.high=std::lround(high->valuedouble*10);next.rules.mode=static_cast<circulation::Mode>(mode->valueint);next.rules.schedule=cJSON_IsTrue(schedule);
        for(unsigned day=0;day<7;day++){auto *row=cJSON_GetArrayItem(week,day);if(!cJSON_IsArray(row)||cJSON_GetArraySize(row)!=6){error="Nieprawidlowy tydzien";return false;}for(unsigned byte=0;byte<6;byte++){auto *v=cJSON_GetArrayItem(row,byte);if(!cJSON_IsNumber(v)||v->valuedouble!=v->valueint||v->valueint<0||v->valueint>255){error="Nieprawidlowy tydzien";return false;}next.rules.week[day][byte]=v->valueint;}}
        if(!next.credentials||!*next.identity){error="Najpierw polacz i potwierdz Tapo";return false;}
        next.armed=true;
    }else{error="Nieznane polecenie pompki";return false;}
    if(!circulation::valid(next.rules)){error="Nieprawidlowe progi";return false;}
    snprintf(next.requests[next.request_cursor],33,"%s",request_id.c_str());next.request_cursor=(next.request_cursor+1)%8;
    next.revision++;
    updates::FlashGuard flash;if(!flash.acquired||!store(next)){error="Nie zapisano konfiguracji; trwa aktualizacja lub blad pamieci";return false;}
    {Lock l(mutex);config=next;if(action=="connect"){known=linked=false;alias.clear();model.clear();protocol.clear();}if(!next.enabled){known=linked=pending=false;decision={};}status=next.enabled?"Oczekiwanie na odczyt Tapo":"Obsluga cyrkulacji wylaczona";}
    if(!start_worker()){error="Brak pamieci na zadanie Tapo";return false;}return true;
}
cJSON *snapshot(){
    auto *out=cJSON_CreateObject();if(!out)return nullptr;
    double temp;uint64_t age;bool sensor=boiler::water_heater_temperature(temp,age);
    Lock l(mutex);bool fresh=linked&&known&&now()-verified_ms<=20000;
    cJSON_AddBoolToObject(out,"enabled",config.enabled);
    cJSON_AddBoolToObject(out,"configured",config.credentials);cJSON_AddBoolToObject(out,"paired",*config.identity);cJSON_AddBoolToObject(out,"armed",config.armed);
    cJSON_AddStringToObject(out,"host",config.host);cJSON_AddStringToObject(out,"alias",alias.c_str());cJSON_AddStringToObject(out,"model",model.c_str());
    cJSON_AddStringToObject(out,"protocol",protocol.c_str());cJSON_AddBoolToObject(out,"credentials_need_refresh",config.credentials&&!config.has_shadow);
    cJSON_AddStringToObject(out,"auth_stage",config.auth_stage);cJSON_AddNumberToObject(out,"auth_http",config.auth_http);cJSON_AddNumberToObject(out,"auth_code",config.auth_code);
    cJSON_AddNumberToObject(out,"auth_derive_ms",config.derive_ms);cJSON_AddNumberToObject(out,"auth_ms",config.auth_ms);
    cJSON_AddNumberToObject(out,"task_stack_free_bytes",worker?uxTaskGetStackHighWaterMark(worker):0);
    cJSON_AddStringToObject(out,"mac",config.mac);cJSON_AddBoolToObject(out,"scan_busy",scan_busy||scan_requested);cJSON_AddNumberToObject(out,"scan_sequence",scan_sequence);
    auto *devices=cJSON_AddArrayToObject(out,"devices");for(const auto &device:discovered){auto *v=cJSON_CreateObject();cJSON_AddStringToObject(v,"host",device.host.c_str());cJSON_AddStringToObject(v,"mac",device.mac.c_str());cJSON_AddStringToObject(v,"model",device.model.c_str());cJSON_AddStringToObject(v,"protocol",device.protocol.c_str());cJSON_AddItemToArray(devices,v);}
    cJSON_AddStringToObject(out,"status",status.c_str());cJSON_AddBoolToObject(out,"fresh",fresh);cJSON_AddBoolToObject(out,"pending",pending);
    if(fresh)cJSON_AddBoolToObject(out,"on",actual_on);else cJSON_AddNullToObject(out,"on");
    if(sensor)cJSON_AddNumberToObject(out,"temperature",temp);else cJSON_AddNullToObject(out,"temperature");
    cJSON_AddBoolToObject(out,"sensor_fresh",sensor);cJSON_AddStringToObject(out,"reason",reason(decision.reason));
    cJSON_AddNumberToObject(out,"lower",config.rules.low/10.0);cJSON_AddNumberToObject(out,"upper",config.rules.high/10.0);
    cJSON_AddNumberToObject(out,"mode",static_cast<unsigned>(config.rules.mode));cJSON_AddBoolToObject(out,"schedule_enabled",config.rules.schedule);
    cJSON_AddNumberToObject(out,"revision",config.revision);cJSON_AddNumberToObject(out,"write_count",writes);
    auto *meter=cJSON_AddObjectToObject(out,"energy");bool supported=tapo_discovery::metered_model(model);
    bool meter_fresh=fresh&&energy_ms&&now()-energy_ms<=90000;
    tapo_energy::Days current_days;bool dates=tapo_energy::range(time(nullptr),current_days);
    bool day_fresh=meter_fresh&&dates&&energy_epoch>=current_days.start[6]&&energy_epoch<current_days.start[7];
    bool history_fresh=fresh&&dates&&energy_history_ms&&now()-energy_history_ms<=1200000&&energy_days.start[6]==current_days.start[6];
    cJSON_AddBoolToObject(meter,"supported",supported);cJSON_AddBoolToObject(meter,"fresh",meter_fresh);cJSON_AddBoolToObject(meter,"history_fresh",history_fresh);
    auto numeric=[&](const char *key,double value){if(std::isfinite(value))cJSON_AddNumberToObject(meter,key,value);else cJSON_AddNullToObject(meter,key);};
    numeric("power_w",meter_fresh?energy.power:NAN);numeric("today_kwh",day_fresh?energy.today:NAN);numeric("month_kwh",day_fresh?energy.month:NAN);
    numeric("week_kwh",day_fresh&&history_fresh?tapo_energy::week(energy_days,energy.today):NAN);
    auto *daily=cJSON_AddArrayToObject(meter,"days");
    if(dates)for(unsigned day=0;day<7;day++){auto *item=cJSON_CreateObject();tm date{};localtime_r(&current_days.start[day],&date);char label[11];strftime(label,sizeof(label),"%Y-%m-%d",&date);cJSON_AddStringToObject(item,"date",label);
        double value=day==6?(day_fresh?energy.today:NAN):(history_fresh?energy_days.kwh[day]:NAN);if(std::isfinite(value))cJSON_AddNumberToObject(item,"kwh",value);else cJSON_AddNullToObject(item,"kwh");cJSON_AddItemToArray(daily,item);}
    cJSON_AddBoolToObject(out,"clock_valid",time(nullptr)>=1735689600);
    auto *week=cJSON_AddArrayToObject(out,"week");for(auto &day:config.rules.week){auto *row=cJSON_CreateArray();for(auto byte:day)cJSON_AddItemToArray(row,cJSON_CreateNumber(byte));cJSON_AddItemToArray(week,row);}
    return out;
}
}
