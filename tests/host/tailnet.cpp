#include "tailnet_guard.hpp"
#include <array>
#include <cassert>
#include <string>
#include <cstdio>
void tailnet_tests(){
    using namespace tailnet_guard;
    std::array<uint8_t,16> mapped{};mapped[10]=mapped[11]=255;mapped[12]=192;mapped[13]=168;mapped[14]=1;mapped[15]=10;
    assert(mapped_ipv4(mapped.data(),16)==0xc0a8010a&&local_configuration(mapped_ipv4(mapped.data(),16)));
    mapped[12]=100;mapped[13]=64;assert(!local_configuration(mapped_ipv4(mapped.data(),16)));
    mapped[12]=127;mapped[13]=mapped[14]=0;mapped[15]=1;assert(local_configuration(mapped_ipv4(mapped.data(),16)));
    for(size_t n=0;n<16;++n)assert(!mapped_ipv4(mapped.data(),n));
    mapped[0]=0xfd;assert(!mapped_ipv4(mapped.data(),16));assert(!local_configuration(0));
    assert(can_start(start_free,start_block,start_psram));
    assert(!can_start(start_free-1,start_block,start_psram));
    assert(!can_start(start_free,start_block-1,start_psram));
    assert(!can_start(start_free,start_block,start_psram-1));
    assert(memory_low(48*1024-1,1024*1024));
    assert(memory_low(128*1024,512*1024-1));
    assert(!memory_low(48*1024,512*1024));
    const std::string key="tskey-auth-"+std::string(40,'a');
    assert(auth_key(key.c_str()));assert(!auth_key(nullptr));assert(!auth_key(""));
    assert(!auth_key("tskey-api-abcdefghijklmnop"));
    assert(!auth_key((key+"\n").c_str()));assert(!auth_key((key+"!").c_str()));
    assert(!auth_key(("tskey-auth-"+std::string(200,'a')).c_str()));
    std::array<uint8_t,80> valid{};valid[0]=0x45;valid[3]=40;valid[9]=6;
    valid[12]=100;valid[13]=64;valid[15]=2;valid[16]=100;valid[17]=64;valid[19]=1;
    valid[23]=80;valid[32]=0x50;
    constexpr uint32_t self=0x64400001;
    const auto allowed=[&](const auto&p,size_t header=40,size_t total=40){return packet(p.data(),header,total,self,true);};
    assert(allowed(valid));assert(!packet(valid.data(),40,40,self,false));
    for(size_t padding=1;padding<=15;++padding)assert(allowed(valid,40,40+padding));
    assert(!allowed(valid,40,56));
    {auto p=valid;p[32]=0xf0;assert(!allowed(p,40,55));} // Padding is not TCP options/data.
    {auto p=valid;p[23]=22;assert(!allowed(p,40,48));} // Padding never broadens the port gate.
    assert(!packet(valid.data(),40,40,self+1,true));assert(!packet(valid.data(),40,40,0,true));
    for(size_t n=0;n<40;++n){assert(!allowed(valid,n));assert(!allowed(valid,40,n));}
    for(unsigned port:{0,22,53,443,8899,65535}){auto p=valid;p[22]=port>>8;p[23]=port;assert(!allowed(p));}
    for(unsigned proto:{0,1,17,58,255}){auto p=valid;p[9]=proto;assert(!allowed(p));}
    for(unsigned fragment:{1,0x2000,0x3fff}){auto p=valid;p[6]=fragment>>8;p[7]=fragment;assert(!allowed(p));}
    {auto p=valid;p[6]=0x40;assert(allowed(p));} // DF is harmless.
    for(unsigned ihl:{0,4,6,15}){auto p=valid;p[0]=0x40|ihl;assert(!allowed(p));}
    {auto p=valid;p[0]=0x65;assert(!allowed(p));}
    {auto p=valid;p[3]=41;assert(!allowed(p));}
    {auto p=valid;p[32]=0x40;assert(!allowed(p));p[32]=0xf0;assert(!allowed(p));}
    for(unsigned first:{0,10,127,192,255}){auto p=valid;p[12]=first;assert(!allowed(p));}
    for(unsigned second:{0,63,128,255}){auto p=valid;p[13]=second;assert(!allowed(p));}
    assert(!vpn_ip(0x643fffff)&&vpn_ip(0x64400000)&&vpn_ip(0x647fffff)&&!vpn_ip(0x64800000));
    {auto p=valid;p[0]=0x4f;p[3]=80;p[23]=0;p[63]=80;p[72]=0x50;assert(allowed(p,80,80));}
    puts("PASS Tailnet: heap reserve, auth key syntax, exact panel address/port, malformed/truncated IP, fragments and non-TCP rejected");
}
