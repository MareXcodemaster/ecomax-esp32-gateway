#include "boiler_clock_codec.hpp"
#include <cassert>
#include <cstdio>
void boiler_clock_tests(){
    uint8_t observed[26]={'T','I','M','E',3,30,15,0xea,7,10,25,7},out[26]{};
    for(unsigned i=12;i<26;++i)observed[i]=30+i;
    tm local{};local.tm_year=126;local.tm_mon=9;local.tm_mday=25;local.tm_hour=2;local.tm_min=30;local.tm_sec=15;
    assert(boiler_clock_codec::valid(observed,26));
    assert(boiler_clock_codec::encode(observed,26,local,out));
    assert(boiler_clock_codec::same_metadata(observed,out));
    assert(boiler_clock_codec::civil(out)-boiler_clock_codec::civil(observed)==-3600);
    local.tm_hour=4;assert(boiler_clock_codec::encode(observed,26,local,out));
    assert(boiler_clock_codec::civil(out)-boiler_clock_codec::civil(observed)==3600);
    local.tm_mday=26;assert(!boiler_clock_codec::encode(observed,26,local,out));
    assert(!boiler_clock_codec::valid(nullptr,26));for(unsigned n=0;n<26;++n)assert(!boiler_clock_codec::valid(observed,n));
    observed[7]=0xe8;observed[9]=2;observed[10]=29;assert(boiler_clock_codec::valid(observed,26));
    observed[7]=0xe9;assert(!boiler_clock_codec::valid(observed,26));
    assert(boiler_clock_codec::civil(2026,11,8,0,0,0)-boiler_clock_codec::civil(2026,11,7,23,59,59)==1);
    assert(boiler_clock_codec::civil(2024,3,1,0,0,0)-boiler_clock_codec::civil(2024,2,28,0,0,0)==172800);
    puts("PASS boiler clock: DST civil-time corrections, preserved opaque fields/weekday, date and truncation guards");
}
