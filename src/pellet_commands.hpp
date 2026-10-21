#pragma once
#include <cstddef>
#include <cstdint>

namespace pellet_commands {
inline bool point(unsigned percent,uint8_t (&payload)[3]) {
    if(percent!=0&&percent!=100)return false;
    payload[0]=1;payload[1]=140;payload[2]=percent==100?0:1;return true;
}
inline bool write_allowed(const uint8_t *payload,size_t size,unsigned response) {
    return payload&&size==3&&response==240&&payload[0]==1&&payload[1]==140&&payload[2]<=1;
}
inline bool packet(const uint8_t *bytes,size_t size,unsigned type) {
    if(!bytes||size<10||bytes[0]!=0x68||(bytes[1]|(bytes[2]<<8))!=size||
       bytes[4]!=69||(bytes[3]!=0&&bytes[3]!=86)||bytes[7]!=type||bytes[size-1]!=0x16)return false;
    uint8_t sum=0;for(size_t i=0;i<size-2;++i)sum^=bytes[i];return sum==bytes[size-2];
}
inline bool configuration(const uint8_t *bytes,size_t size) {
    constexpr unsigned offset=10+140*3;
    return size==546&&packet(bytes,size,225)&&bytes[8]==178&&bytes[9]==0&&
           bytes[offset]==2&&bytes[offset+1]==0&&bytes[offset+2]==2;
}
inline bool acknowledged(const uint8_t *bytes,size_t size) {
    return size==10&&packet(bytes,size,240);
}
}
