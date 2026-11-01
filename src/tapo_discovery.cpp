#include "tapo_discovery.hpp"
#include "cJSON.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lwip/sockets.h"
#include <algorithm>
#include <cstring>
namespace tapo_discovery {
std::vector<Device> scan(){
    std::vector<Device> result;
    auto *net=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");esp_netif_ip_info_t info{};
    if(!net||esp_netif_get_ip_info(net,&info)!=ESP_OK||!info.ip.addr)return result;
    int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0)return result;
    int yes=1;setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));timeval timeout{0,250000};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    sockaddr_in target{};target.sin_family=AF_INET;target.sin_port=htons(20002);target.sin_addr.s_addr=info.ip.addr|~info.netmask.addr;
    const unsigned char probe[]={2,0,0,1,0,0,0,0,0,0,0,0,0x46,0x3c,0xb5,0xd3};
    sendto(fd,probe,sizeof(probe),0,reinterpret_cast<sockaddr *>(&target),sizeof(target));
    auto until=esp_timer_get_time()+2500000;
    auto *packet=static_cast<char *>(heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!packet){close(fd);return result;}
    while(esp_timer_get_time()<until&&result.size()<16){
        sockaddr_in from{};socklen_t size=sizeof(from);int n=recvfrom(fd,packet,4096,0,reinterpret_cast<sockaddr *>(&from),&size);
        if(n<17||static_cast<unsigned char>(packet[0])!=2||from.sin_port!=htons(20002)||
           ((from.sin_addr.s_addr^info.ip.addr)&info.netmask.addr)!=0)continue;
        unsigned length=(static_cast<unsigned char>(packet[4])<<8)|static_cast<unsigned char>(packet[5]);if(length!=static_cast<unsigned>(n-16))continue;
        auto *body=cJSON_ParseWithLength(packet+16,n-16),*r=cJSON_GetObjectItemCaseSensitive(body,"result");
        auto string=[&](const char *key){auto *v=cJSON_GetObjectItemCaseSensitive(r,key);return cJSON_IsString(v)?std::string(v->valuestring):std::string{};};
        Device d;d.model=string("device_model");d.mac=normalize_mac(string("mac"));
        if((d.model=="P110"||d.model.rfind("P110(",0)==0)&&!d.mac.empty()){
            char ip[INET_ADDRSTRLEN];inet_ntop(AF_INET,&from.sin_addr,ip,sizeof(ip));d.host=ip;
            auto *scheme=cJSON_GetObjectItemCaseSensitive(r,"mgt_encrypt_schm"),*type=cJSON_GetObjectItemCaseSensitive(scheme,"encrypt_type");
            if(cJSON_IsString(type))d.protocol=type->valuestring;
            if(std::none_of(result.begin(),result.end(),[&](const Device &old){return old.mac==d.mac;}))result.push_back(d);
        }
        cJSON_Delete(body);
    }
    heap_caps_free(packet);close(fd);return result;
}
}
