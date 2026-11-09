#include "schedule_run.hpp"
#include "boiler_clock_codec.hpp"
#include <cassert>
#include <cstdio>
using namespace schedule_run;
void schedule_run_tests(){
    const int64_t monday=boiler_clock_codec::civil(2026,11,9,0,0,0);
    assert(slot(monday)==48);assert(slot(monday-86400)==0);
    Week week;week.meta[0]=1;week.meta[3]=20;
    // Daily 15:00 through midnight to 01:30. Start Monday 12:16:49.
    for(unsigned day=0;day<7;++day)for(unsigned s=0;s<48;++s)if(s<3||s>=30)permit(week,day*48+s);
    const int64_t start=monday+12*3600+16*60+49;Plan p;
    assert(plan(week,start,p)==Result::ready);
    assert(p.resume==monday+15*3600&&p.end==monday+86400+5400);
    for(unsigned s=0;s<336;++s)assert(on(p.extended,s)==(on(week,s)||(s>=48+24&&s<48+30)));
    assert(!on(p.extended,slot(p.end)));assert(!memcmp(p.extended.meta,week.meta,4));
    assert(plan(week,monday+15*3600,p)==Result::already_allowed);
    assert(plan(week,monday+5400,p)==Result::ready); // Exact OFF boundary.
    assert(plan(week,monday+14*3600+59*60+59,p)==Result::ready); // Last second of gap.
    assert(p.resume==monday+15*3600);
    assert(plan(week,monday-86400+12*3600,p)==Result::ready); // Sunday -> Monday.
    assert(p.end==monday+5400);
    Week empty;empty.meta[0]=1;
    assert(plan(empty,start,p)==Result::missing_window);
    empty.meta[0]=0;assert(plan(empty,start,p)==Result::disabled);
    empty.meta[0]=2;assert(plan(empty,start,p)==Result::invalid);
    Week almost;almost.meta[0]=1;for(unsigned s=0;s<336;++s)if(s!=slot(start))permit(almost,s);
    assert(plan(almost,start,p)==Result::missing_end); // Never turn the whole week ON.
    assert(plan(week,0,p)==Result::invalid);
    assert(plan(week,start,p)==Result::ready);
    Record record;record.plan=p;strcpy(record.identity,"TEST-BOILER");assert(valid(record));
    assert(decide(record,week,true,start,false)==Decision::finish); // Crash before dispatch.
    assert(decide(record,p.extended,true,start,false)==Decision::restore); // Uncertain dispatch, no START replay.
    record.stage=Stage::active;
    Record reboot;memcpy(&reboot,&record,sizeof(record));assert(valid(reboot));
    assert(decide(reboot,p.extended,true,start+30,false)==Decision::wait);
    assert(decide(reboot,p.extended,true,start+30,true)==Decision::restore); // STOP / no confirmed START after reboot.
    assert(decide(reboot,p.extended,false,0,false)==Decision::restore);
    assert(decide(reboot,p.extended,true,start-3600,false)==Decision::restore); // DST/manual backwards correction.
    assert(decide(reboot,p.extended,true,p.resume,false)==Decision::restore);
    assert(decide(reboot,p.extended,true,p.end+7*86400,false)==Decision::restore); // Long outage.
    assert(decide(reboot,week,true,start+30,false)==Decision::external_change);
    Week external=p.extended;external.days[3][1]^=1;
    assert(decide(reboot,external,true,p.resume,false)==Decision::external_change);
    reboot.stage=Stage::watching;
    assert(decide(reboot,week,true,p.resume,false)==Decision::watch);
    assert(decide(reboot,week,true,p.end,false)==Decision::finish);
    reboot.stage=Stage::restoring;assert(decide(reboot,p.extended,true,start,false)==Decision::restore);
    assert(decide(reboot,week,true,start,false)==Decision::finish);
    reboot.plan.end+=1;assert(!valid(reboot));
    puts("PASS one-shot schedule: next window, midnight/week boundary, native OFF boundary, persistence, external edits, clock jumps, STOP and uncertain writes");
}
