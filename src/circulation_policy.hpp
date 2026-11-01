#pragma once
#include <cmath>
#include <cstdint>
namespace circulation {
enum class Mode:uint8_t { off, automatic, on, manual_off };
enum class Reason:uint8_t { inactive, invalid, off, sensor, clock, schedule, cold, hysteresis, cooldown, manual, warm, manual_off };
struct Rules {
    int16_t low=400,high=450;
    Mode mode=Mode::off;
    bool schedule=false;
    uint8_t week[7][6]{};
};
inline bool valid(const Rules &r) {
    return r.low>=50&&r.high<=850&&r.high-r.low>=5&&static_cast<unsigned>(r.mode)<=3;
}
inline bool in_window(const Rules &r,unsigned day,unsigned slot) {
    return day<7&&slot<48&&(r.week[day][slot/8]&(1U<<(7-slot%8)));
}
struct Input {
    bool armed=false,fresh=false,clock=false;
    double temperature=NAN;
    unsigned day=0,slot=0;
    uint64_t now_ms=0,last_off_ms=0;
};
struct Decision {bool manage=false,on=false;Reason reason=Reason::inactive;};
struct Policy {
    bool demand=false;
    Decision step(const Rules &r,const Input &i) {
        Decision d;d.manage=i.armed;
        if(!i.armed){demand=false;return d;}
        if(!valid(r)){demand=false;d.reason=Reason::invalid;return d;}
        if(r.mode==Mode::off||r.mode==Mode::manual_off){demand=false;d.reason=r.mode==Mode::manual_off?Reason::manual_off:Reason::off;return d;}
        if(!i.fresh||!std::isfinite(i.temperature)||i.temperature<0||i.temperature>100){demand=false;d.reason=Reason::sensor;return d;}
        if(i.temperature*10<=r.low)demand=false;
        else if(i.temperature*10>=r.high)demand=true;
        if(r.mode==Mode::automatic&&r.schedule){
            if(!i.clock){d.reason=Reason::clock;return d;}
            if(!in_window(r,i.day,i.slot)){d.reason=Reason::schedule;return d;}
        }
        d.on=r.mode==Mode::on||demand;
        d.reason=r.mode==Mode::on?Reason::manual:d.on?Reason::warm:i.temperature*10<=r.low?Reason::cold:Reason::hysteresis;
        if(r.mode==Mode::automatic&&d.on&&(i.now_ms<i.last_off_ms||i.now_ms-i.last_off_ms<30000)){d.on=false;d.reason=Reason::cooldown;}
        return d;
    }
};
}
