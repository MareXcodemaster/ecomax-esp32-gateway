#pragma once
#include <cstring>
namespace identity_guard {
enum class Action { reject, match, enroll };
inline Action decide(const char *stored,const char *candidate,bool supported_model) {
    if (!supported_model||!stored||!candidate) return Action::reject;
    size_t length=std::strlen(candidate);
    if (!length||length>160) return Action::reject;
    for(size_t i=0;i<length;++i)
        if (!((candidate[i]>='0'&&candidate[i]<='9')||(candidate[i]>='A'&&candidate[i]<='Z'))) return Action::reject;
    return !*stored?Action::enroll:!std::strcmp(stored,candidate)?Action::match:Action::reject;
}
}
