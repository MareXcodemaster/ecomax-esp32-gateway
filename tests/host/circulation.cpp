#include "circulation_policy.hpp"
#include "tapo_discovery.hpp"
#include "tapo_energy.hpp"
#include "tapo_session.hpp"
#include <cassert>
void circulation_tests() {
    {
        struct Config {char host[16]="192.0.2.20",identity[65]="fixture",mac[13]="021122334455";uint8_t auth[32]{},shadow[20]{};bool credentials=true,has_shadow=true,has_klap=true;circulation::Rules rules;unsigned revision=1;} c;
        tapo_session::Key key;assert(!key.matches(c));key.capture(c);assert(key.matches(c));
        c.rules.mode=circulation::Mode::on;c.rules.high=460;c.rules.schedule=true;c.rules.week[1][2]=128;++c.revision;assert(key.matches(c));
        auto changed=c;changed.host[9]='1';assert(!key.matches(changed));changed=c;changed.identity[0]='x';assert(!key.matches(changed));
        changed=c;changed.mac[0]='1';assert(!key.matches(changed));changed=c;changed.auth[0]=1;assert(!key.matches(changed));
        changed=c;changed.shadow[0]=1;assert(!key.matches(changed));changed=c;changed.has_klap=false;assert(!key.matches(changed));
        assert(tapo_session::retry_delay(0)==1000&&tapo_session::retry_delay(1)==3000&&tapo_session::retry_delay(2)==7000&&tapo_session::retry_delay(100)==30000);
    }
    assert(tapo_discovery::normalize_mac("02:11:aa:BB:44:55")=="0211AABB4455");
    assert(tapo_discovery::normalize_mac("02-11-AA-BB-44-55")=="0211AABB4455");
    assert(tapo_discovery::normalize_mac("bad").empty());
    for(auto model:{"P100","P100(EU)","P105","P110","P110(FR)","P110M","P115"})assert(tapo_discovery::supported_model(model));
    for(auto model:{"","P110X","P110()","P110(EU","P110(EU)garbage","L530","P300","P999"})assert(!tapo_discovery::supported_model(model));
    assert(tapo_discovery::metered_model("P110(FR)")&&!tapo_discovery::metered_model("P100"));
    {
        auto *data=cJSON_Parse(R"({"current_power":3999,"today_energy":10,"month_energy":120})");
        auto value=tapo_energy::usage(data);assert(value.power==3.999&&value.today==.010&&value.month==.120);cJSON_Delete(data);
        data=cJSON_Parse(R"({"current_power":-1,"today_energy":null})");value=tapo_energy::usage(data);assert(std::isnan(value.power)&&std::isnan(value.today));cJSON_Delete(data);
        tapo_energy::Days days;assert(!tapo_energy::range(0,days));assert(tapo_energy::range(1793570400,days));
        // Six complete days, including a 23-hour DST day, and a live today total.
        days.start[0]=1700000000;for(unsigned day=0;day<7;day++)days.start[day+1]=days.start[day]+(day==2?23:24)*3600;
        data=cJSON_CreateObject();cJSON_AddNumberToObject(data,"start_timestamp",days.start[0]);cJSON_AddNumberToObject(data,"interval",60);
        auto *hours=cJSON_AddArrayToObject(data,"data");for(unsigned hour=0;hour<143;hour++)cJSON_AddItemToArray(hours,cJSON_CreateNumber(100));
        assert(tapo_energy::hourly(data,days));assert(days.kwh[2]==2.3&&std::abs(tapo_energy::week(days,.01)-14.31)<.00001);
        cJSON_DeleteItemFromArray(hours,100);tapo_energy::Days short_days;short_days.start=days.start;assert(tapo_energy::hourly(data,short_days));assert(std::isnan(tapo_energy::week(short_days,.01)));
        cJSON_ReplaceItemInObject(data,"interval",cJSON_CreateNumber(1440));assert(!tapo_energy::hourly(data,days));cJSON_Delete(data);
    }
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
    i.now_ms=i.last_off_ms;r.mode=Mode::on;assert(p.step(r,i).on); // Manual ON bypasses automatic anti-cycling.
    i.fresh=false;assert(p.step(r,i).reason==Reason::sensor);i.fresh=true;
    r.mode=Mode::manual_off;i.fresh=false;assert(!p.step(r,i).on&&p.step(r,i).reason==Reason::manual_off);i.fresh=true;
    r.low=450;assert(!valid(r));assert(p.step(r,i).reason==Reason::invalid);
    r.low=400;i.armed=false;assert(!p.step(r,i).manage);
    for(unsigned day=0;day<7;day++)for(unsigned slot=0;slot<48;slot++){
        Rules w;w.week[day][slot/8]=1U<<(7-slot%8);
        assert(in_window(w,day,slot));assert(!in_window(w,7,slot));assert(!in_window(w,day,48));
    }
}
