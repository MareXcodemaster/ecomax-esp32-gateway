#pragma once
#include <cstdint>
#include <cstring>

// Sunday first, MSB first, 48 half-hours/day: the existing 54/182 and 55 format.
// Civil time comes from the boiler, not from the browser or UTC.
namespace schedule_run {
constexpr int64_t half_hour=1800,week_seconds=7*86400;
struct Week { uint8_t days[7][6]{},meta[4]{}; };
inline bool valid(const Week &w){return w.meta[0]<=1&&w.meta[2]<=w.meta[1]&&w.meta[1]<=w.meta[3]&&w.meta[3]<=100;}
inline unsigned slot(int64_t civil){return ((civil/86400+4)%7)*48+(civil%86400)/half_hour;}
inline bool on(const Week &w,unsigned s){s%=336;return w.days[s/48][s%48/8]&(1<<(7-s%8));}
inline void permit(Week &w,unsigned s){s%=336;w.days[s/48][s%48/8]|=1<<(7-s%8);}
inline bool same(const Week &a,const Week &b){return !memcmp(&a,&b,sizeof(a));}
struct Plan { Week before{},extended{}; int64_t start=0,resume=0,end=0; };
enum class Result { ready,disabled,already_allowed,missing_window,missing_end,invalid };
inline Result plan(const Week &w,int64_t civil,Plan &p){
    p={};if(!valid(w)||civil<1704067200LL||civil>4102444799LL)return Result::invalid;
    if(!w.meta[0])return Result::disabled;
    unsigned first=slot(civil);if(on(w,first))return Result::already_allowed;
    unsigned gap=1;while(gap<336&&!on(w,first+gap))++gap;
    if(gap==336)return Result::missing_window;
    unsigned end=gap+1;while(end<gap+336&&on(w,first+end))++end;
    Plan next;next.before=w;next.extended=w;next.start=civil;
    next.resume=civil-civil%half_hour+gap*half_hour;
    next.end=civil-civil%half_hour+end*half_hour;
    for(unsigned i=0;i<gap;++i)permit(next.extended,first+i);
    // The weekly representation must retain an OFF boundary even without ESP.
    if(end>=336||on(next.extended,first+end))return Result::missing_end;
    p=next;return Result::ready;
}
enum class Stage:uint8_t { prepared=1,active=2,restoring=3,watching=4 };
struct Record { uint32_t version=1; Stage stage=Stage::prepared; char identity[161]{}; Plan plan{}; };
inline bool valid(const Record &r){
    Plan p;return r.version==1&&r.stage>=Stage::prepared&&r.stage<=Stage::watching&&r.identity[0]&&
        memchr(r.identity,0,sizeof(r.identity))&&plan(r.plan.before,r.plan.start,p)==Result::ready&&
        p.resume==r.plan.resume&&p.end==r.plan.end&&same(p.extended,r.plan.extended);
}
enum class Decision { wait,restore,watch,finish,external_change };
inline Decision decide(const Record &r,const Week &actual,bool clock_fresh,int64_t civil,bool stopped){
    if(!same(actual,r.plan.before)&&!same(actual,r.plan.extended))return Decision::external_change;
    bool cancel=r.stage==Stage::prepared||r.stage==Stage::restoring||!clock_fresh||civil<r.plan.start-5||stopped;
    if(same(actual,r.plan.before)){
        if(cancel||civil>=r.plan.end)return Decision::finish;
        if(r.stage==Stage::active&&civil<r.plan.resume)return Decision::external_change;
        return Decision::watch;
    }
    return cancel||civil>=r.plan.resume?Decision::restore:Decision::wait;
}
}
