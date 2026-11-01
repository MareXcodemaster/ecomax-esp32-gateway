// Private stdin/stdout bridge for the read-only device probe. No credentials in argv.
#include "smart_tapo.hpp"
#include "cJSON.h"
#include <chrono>
#include <iostream>
using tapo_tpap::Bytes;
std::string hex(const std::string &value){const char *d="0123456789abcdef";std::string out;for(unsigned char c:value){out+=d[c>>4];out+=d[c&15];}return out;}
void emit(cJSON *value){char *s=cJSON_PrintUnformatted(value);if(!s)std::exit(2);std::cout<<s<<std::endl;cJSON_free(s);cJSON_Delete(value);}
int random_bytes(void *,unsigned char *out,size_t size){
    auto *value=cJSON_CreateObject();cJSON_AddNumberToObject(value,"random",size);emit(value);
    std::string input;if(!(std::cin>>input))return -1;auto data=tapo_tpap::hex(input.c_str());if(data.size()!=size)return -1;
    std::copy(data.begin(),data.end(),out);return 0;
}
void esp_fill_random(void *buffer,size_t size){if(random_bytes(nullptr,static_cast<unsigned char *>(buffer),size))std::exit(3);}
uint64_t clock_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Http:smart_tapo::Transport {
    Response request(const std::string &path,const std::string &body,const char *type,const std::string &cookie){
        auto *value=cJSON_CreateObject();cJSON_AddStringToObject(value,"path",path.c_str());cJSON_AddStringToObject(value,"body",hex(body).c_str());
        cJSON_AddStringToObject(value,"type",type);cJSON_AddStringToObject(value,"cookie",cookie.c_str());emit(value);
        std::string line;if(!std::getline(std::cin>>std::ws,line))return {};auto *reply=cJSON_Parse(line.c_str());if(!reply)return {};
        auto *status=cJSON_GetObjectItem(reply,"status"),*data=cJSON_GetObjectItem(reply,"body"),*set_cookie=cJSON_GetObjectItem(reply,"cookie");Response out;
        if(cJSON_IsNumber(status))out.status=status->valueint;
        if(cJSON_IsString(data)){auto decoded=tapo_tpap::hex(data->valuestring);out.body.assign(decoded.begin(),decoded.end());}
        if(cJSON_IsString(set_cookie))out.set_cookie=set_cookie->valuestring;
        cJSON_Delete(reply);return out;
    }
    Response exchange(const std::string &path,const std::string &body,const char *type)override{return request(path,body,type,{});}
    Response post(const std::string &path,const std::string &body,const std::string &cookie)override{return request(path,body,"application/octet-stream",cookie);}
    void reset()override{}
};
int main(){
    std::string auth,shadow;if(!(std::cin>>auth>>shadow))return 1;
    Http http;smart_tapo::Client client(random_bytes,nullptr,nullptr,clock_ms);client.set_transport(&http);
    auto *out=cJSON_CreateObject();bool ok=client.handshake(tapo_tpap::hex(auth.c_str()),tapo_tpap::hex(shadow.c_str()),true);
    std::string response;if(ok)response=client.send_message(R"({"method":"get_device_info"})");
    cJSON_AddBoolToObject(out,"done",ok&&!response.empty());cJSON_AddStringToObject(out,"protocol",client.protocol());cJSON_AddStringToObject(out,"error",client.error().c_str());
    if(!response.empty())cJSON_AddItemToObject(out,"response",cJSON_Parse(response.c_str()));emit(out);return ok&&!response.empty()?0:1;
}
