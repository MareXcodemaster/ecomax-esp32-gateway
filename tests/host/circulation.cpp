#include "circulation_policy.hpp"
#include "tapo_discovery.hpp"
#include <cassert>
void circulation_tests() {
    assert(tapo_discovery::normalize_mac("02:11:aa:BB:44:55")=="0211AABB4455");
    assert(tapo_discovery::normalize_mac("02-11-AA-BB-44-55")=="0211AABB4455");
    assert(tapo_discovery::normalize_mac("bad").empty());
    using namespace circulation;
    Rules r;r.mode=Mode::automatic;Policy p;Input i;i.armed=true;i.fresh=true;i.temperature=45;i.now_ms=30000;
    assert(p.step(r,i).on);i.temperature=42;assert(p.step(r,i).on);i.temperature=40;assert(!p.step(r,i).on);
    i.temperature=42;assert(!p.step(r,i).on);i.temperature=45;assert(p.step(r,i).on);
    i.fresh=false;assert(!p.step(r,i).on);i.fresh=true;i.temperature=42;assert(!p.step(r,i).on);
    const double invalid[]={NAN,INFINITY,-1.0,101.0};for(double value:invalid){i.temperature=value;assert(!p.step(r,i).on);}
    i.temperature=60;r.schedule=true;i.clock=false;assert(p.step(r,i).reason==Reason::clock);
    i.clock=true;r.week[0][0]=128;i.day=0;i.slot=0;assert(p.step(r,i).on);
    i.slot=1;assert(p.step(r,i).reason==Reason::schedule);r.schedule=false;assert(p.step(r,i).on);
    r.schedule=true;i.clock=false;r.mode=Mode::on;assert(p.step(r,i).on);
    r.mode=Mode::off;assert(!p.step(r,i).on);r.mode=Mode::automatic;r.schedule=false;i.last_off_ms=29999;
    assert(p.step(r,i).reason==Reason::cooldown);i.now_ms=59999;assert(p.step(r,i).on);
    r.low=450;assert(!valid(r));assert(p.step(r,i).reason==Reason::invalid);
    r.low=400;i.armed=false;assert(!p.step(r,i).manage);
    for(unsigned day=0;day<7;day++)for(unsigned slot=0;slot<48;slot++){
        Rules w;w.week[day][slot/8]=1U<<(7-slot%8);
        assert(in_window(w,day,slot));assert(!in_window(w,7,slot));assert(!in_window(w,day,48));
    }
}
