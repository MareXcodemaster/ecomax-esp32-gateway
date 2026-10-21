#pragma once
#include <cmath>

namespace advanced_menu {
struct Definition { int index,unit,offset,maximum; double multiplier; };
inline constexpr Definition definitions[]={
    {7,31,12,1,0},{44,31,19,2,0},{45,31,1,1,0},{46,31,26,1,0},
    {151,8,0,2550,10},{158,31,20,1,0},{161,31,23,1,0},
    {162,31,1,1,0},{166,31,34,2,0}
};
inline bool known(int index) {
    for(const auto &entry:definitions)if(entry.index==index)return true;
    return false;
}
inline bool matches(int index,double unit,double multiplier,double offset,double low,double high) {
    for(const auto &entry:definitions)if(entry.index==index)
        return unit==entry.unit&&multiplier==entry.multiplier&&offset==entry.offset&&
               std::isfinite(low)&&std::isfinite(high)&&low==0&&high==entry.maximum;
    return false;
}
inline bool confirmed(bool advanced,bool installation,bool confirmation) {
    return !advanced||(installation&&confirmation);
}
}
