#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tailnet_guard {
constexpr size_t start_free=112*1024, start_block=16*1024, start_psram=2*1024*1024;
inline bool can_start(size_t free,size_t block,size_t psram){return free>=start_free&&block>=start_block&&psram>=start_psram;}
inline bool memory_low(size_t free,size_t psram){return free<48*1024||psram<512*1024;}
inline bool auth_key(const char *s){
    if(!s||std::strncmp(s,"tskey-auth-",11))return false;
    const auto n=std::strlen(s);if(n<24||n>192)return false;
    for(size_t i=0;i<n;++i)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||(s[i]>='0'&&s[i]<='9')||s[i]=='-'||s[i]=='_'))return false;
    return true;
}
inline uint16_t u16(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
inline uint32_t u32(const uint8_t *p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
inline bool vpn_ip(uint32_t ip){return (ip&0xffc00000u)==0x64400000u;}
inline uint32_t mapped_ipv4(const uint8_t *ip,size_t size){
    if(!ip||size!=16)return 0;
    for(unsigned i=0;i<10;++i)if(ip[i])return 0;
    return ip[10]==255&&ip[11]==255?u32(ip+12):0;
}
inline bool local_configuration(uint32_t ip){return ip!=0&&!vpn_ip(ip);}
// Additional application gate, AFTER WireGuard authentication and Tailnet ACLs.
// Only unfragmented IPv4 TCP directed to this ESP's HTTP listener is admitted.
inline bool packet(const uint8_t *p,size_t header_bytes,size_t total,uint32_t self,bool enabled){
    if(!enabled||!vpn_ip(self)||header_bytes<40||total<40||(p[0]>>4)!=4)return false;
    const unsigned ip_bytes=(p[0]&15)*4;
    const unsigned ip_length=u16(p+2);
    // Decrypted WireGuard packets retain up to 15 bytes of block padding.
    // Validate the real IP length; the caller removes padding before lwIP.
    if(ip_bytes<20||ip_bytes>60||header_bytes<ip_bytes+20||ip_length<ip_bytes+20||ip_length>total||total-ip_length>15)return false;
    if((u16(p+6)&0x3fff)||p[9]!=6||u32(p+16)!=self||!vpn_ip(u32(p+12)))return false;
    const unsigned tcp_bytes=(p[ip_bytes+12]>>4)*4;
    return tcp_bytes>=20&&ip_length>=ip_bytes+tcp_bytes&&u16(p+ip_bytes+2)==80;
}
}
