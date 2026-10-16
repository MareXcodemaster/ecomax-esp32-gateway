#pragma once
#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace history_data {
constexpr unsigned fields=38, limit=720, retention_seconds=3*24*3600, archive_interval=60;
struct Sample { uint32_t time, native; float mode, values[fields]; };
struct Point { Sample sample; uint32_t gap_before=0; };
static_assert(sizeof(Sample)==164,"History format requires 32-bit IEEE floats");
inline uint32_t step(uint32_t from,uint32_t to,unsigned capacity=limit) {
    return std::max<uint32_t>(1,static_cast<uint32_t>((static_cast<uint64_t>(to)-from+capacity)/capacity));
}
// Input is chronological. The last real measurement in each bucket is retained.
inline size_t reduce(Point *points,size_t count,uint32_t from,uint32_t to,uint32_t seconds,uint32_t max_gap=90) {
    size_t written=0;uint32_t bucket=0,previous=0,native=0;
    for(size_t i=0;i<count;++i){auto point=points[i];auto time=point.sample.time;if(time<from||time>to)continue;
        point.gap_before=point.gap_before||(previous&&(time-previous>max_gap||native!=point.sample.native));
        previous=time;native=point.sample.native;auto next=(time-from)/seconds;
        if(written&&next==bucket){point.gap_before|=points[written-1].gap_before;points[written-1]=point;}
        else {points[written++]=point;bucket=next;}
    }
    return written;
}
}
