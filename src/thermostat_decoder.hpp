#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace plum {
// PyPlumIO's 12-parameter thermostat layout, also captured on this ecoMAX.
constexpr unsigned thermostat_parameters = 12;
constexpr uint8_t thermostat_sizes[] = {1,2,2,1,1,1,1,1,1,2,2,2};
constexpr const char *thermostat_names[] = {"mode","party_target_temp","holidays_target_temp",
    "correction","away_timer","airing_timer","party_timer","holidays_timer","hysteresis",
    "day_target_temp","night_target_temp","antifreeze_target_temp"};
inline int thermostat_temperature_index(unsigned position) {
    switch(position) { case 2:return 9; case 3:return 10; case 5:return 1; case 6:return 2; default:return -1; }
}
inline int thermostat_active_index(unsigned mode) {
    switch(mode) { case 1:return 10; case 2:return 9; case 5:return 1; case 6:return 2; case 7:return 11; default:return -1; }
}
struct ThermostatValue { uint16_t value=0, low=0, high=0; bool present=false; };
struct ThermostatSettings {
    unsigned slots=0;
    ThermostatValue profile;
    ThermostatValue values[8][thermostat_parameters]{};
};
enum class ThermostatSync { invalid, pending, ready };
inline ThermostatSync thermostat_sync(const ThermostatSettings &settings,unsigned slot,double mode,double target,double measured) {
    if(slot>=settings.slots||slot>=8||!std::isfinite(mode)||mode<0||mode>7||std::floor(mode)!=mode||
       !std::isfinite(target)||!std::isfinite(measured))return ThermostatSync::invalid;
    const auto &values=settings.values[slot];const auto &raw_mode=values[0];
    const int active=thermostat_active_index(static_cast<unsigned>(mode));
    if(!raw_mode.present||raw_mode.low!=0||raw_mode.high!=7||(raw_mode.value&127)>7||
       (active>=0&&!values[active].present))return ThermostatSync::invalid;
    if((raw_mode.value&127)!=mode||(active>=0&&std::fabs(values[active].value/10.0-target)>=0.0001))return ThermostatSync::pending;
    return ThermostatSync::ready;
}
inline bool decode_thermostats(const uint8_t *data, size_t size, ThermostatSettings &out) {
    out={};
    if(!data || size<6 || data[0]!=0 || data[1]!=0 || data[2]<13 || (data[2]-1)%thermostat_parameters) return false;
    unsigned slots=(data[2]-1)/thermostat_parameters;
    if(slots>8 || size!=6+slots*51) return false;
    auto read=[](const uint8_t *p,unsigned width) -> uint16_t { return width==2?p[0]|(p[1]<<8):p[0]; };
    auto triple=[&](const uint8_t *p,unsigned width,bool mode,ThermostatValue &v) {
        v.value=read(p,width); v.low=read(p+width,width); v.high=read(p+width*2,width);
        unsigned absent=width==2?65535:255;
        v.present=!(v.value==absent && v.low==absent && v.high==absent);
        unsigned value=mode?(v.value&127):v.value;
        return !v.present || (v.low<=value && value<=v.high);
    };
    ThermostatSettings next; next.slots=slots;
    if(!triple(data+3,1,false,next.profile)) return false;
    size_t at=6;
    for(unsigned slot=0;slot<slots;++slot) for(unsigned i=0;i<thermostat_parameters;++i) {
        if(!triple(data+at,thermostat_sizes[i],i==0,next.values[slot][i])) return false;
        at+=thermostat_sizes[i]*3;
    }
    out=next; return true;
}
inline bool thermostat_self_test() {
    if(thermostat_temperature_index(5)!=1||thermostat_temperature_index(6)!=2||
       thermostat_temperature_index(2)!=9||thermostat_temperature_index(3)!=10||
       thermostat_temperature_index(1)!=-1||thermostat_active_index(0)!=-1||
       thermostat_active_index(5)!=1||thermostat_active_index(6)!=2) return false;
    // Captured RESPONSE_THERMOSTAT_PARAMETERS 220; two unused slots are all FF.
    const uint8_t prefix[]={0,0,37,255,255,255,2,0,7,231,0,100,0,94,1,212,0,100,0,94,1,
        109,60,140,6,1,60,4,1,60,57,1,60,16,1,60,6,2,50,220,0,100,0,94,1,
        7,1,100,0,94,1,231,0,50,0,44,1};
    uint8_t bytes[160]; std::fill_n(bytes,sizeof(bytes),255); std::memcpy(bytes,prefix,sizeof(prefix));
    ThermostatSettings out;
    if(!decode_thermostats(bytes,159,out) || out.slots!=3 || out.values[0][9].value!=220 ||
       out.values[0][10].value!=263 || out.values[1][0].present || out.profile.present) return false;
    for(size_t n=0;n<159;++n) if(decode_thermostats(bytes,n,out)) return false;
    if(decode_thermostats(bytes,160,out)) return false;
    bytes[1]=1; if(decode_thermostats(bytes,159,out)) return false; bytes[1]=0;
    bytes[2]=38; if(decode_thermostats(bytes,159,out)) return false; bytes[2]=37;
    bytes[6]=133; if(!decode_thermostats(bytes,159,out) || out.values[0][0].value!=133) return false;
    bytes[6]=8; if(decode_thermostats(bytes,159,out)) return false; bytes[6]=2;
    bytes[39]=99; if(decode_thermostats(bytes,159,out)) return false;
    return true;
}
}
