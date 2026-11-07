#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <ctime>
namespace boiler_clock_codec {
inline bool valid(const uint8_t *p,size_t n) {
    if(!p||n!=26||memcmp(p,"TIME",4))return false;
    const unsigned y=p[7]|(p[8]<<8),m=p[9],d=p[10];
    const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    bool leap=y%4==0&&(y%100!=0||y%400==0);
    return y>=2024&&y<=2099&&m>=1&&m<=12&&d>=1&&d<=days[m-1]+(m==2&&leap?1:0)&&
        p[4]<24&&p[5]<60&&p[6]<60&&p[11]<=7;
}
// Local civil seconds, deliberately independent of DST and libc's ambiguous-hour rules.
inline int64_t civil(unsigned y,unsigned m,unsigned d,unsigned h,unsigned min,unsigned sec) {
    int64_t days=0;for(unsigned year=1970;year<y;++year)days+=365+(year%4==0&&(year%100!=0||year%400==0));
    const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    for(unsigned month=1;month<m;++month)days+=lengths[month-1]+(month==2&&y%4==0&&(y%100!=0||y%400==0));
    return ((days+d-1)*24+h)*3600+min*60+sec;
}
inline int64_t civil(const uint8_t *p){return civil(p[7]|(p[8]<<8),p[9],p[10],p[4],p[5],p[6]);}
inline int64_t civil(const tm &t){return civil(t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec);}
inline bool encode(const uint8_t *observed,size_t n,const tm &local,uint8_t out[26]) {
    if(!valid(observed,n)||local.tm_year+1900!=(observed[7]|(observed[8]<<8))||
       local.tm_mon+1!=observed[9]||local.tm_mday!=observed[10]||local.tm_hour<0||local.tm_hour>23||
       local.tm_min<0||local.tm_min>59||local.tm_sec<0||local.tm_sec>59)return false;
    // Only clock fields are proven. Preserve date, weekday and all opaque metadata.
    memcpy(out,observed,26);out[4]=local.tm_hour;out[5]=local.tm_min;out[6]=local.tm_sec;return true;
}
inline bool same_metadata(const uint8_t *a,const uint8_t *b){return !memcmp(a,b,4)&&!memcmp(a+7,b+7,19);}
}
