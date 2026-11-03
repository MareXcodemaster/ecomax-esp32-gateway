#pragma once
#include <cstring>
namespace identity_guard {
enum class Action { reject, match, enroll };
inline const char *rejection(const char *stored,const char *candidate,bool supported_model){
    if(!supported_model)return "unsupported_model";
    if(!stored||!candidate||!*candidate||std::strlen(candidate)>160)return "invalid_identity";
    for(const char *p=candidate;*p;++p)if(!((*p>='0'&&*p<='9')||(*p>='A'&&*p<='Z')))return "invalid_identity";
    return *stored&&std::strcmp(stored,candidate)?"identity_mismatch":nullptr;
}
inline Action decide(const char *stored,const char *candidate,bool supported_model) {
    if(rejection(stored,candidate,supported_model))return Action::reject;
    return !*stored?Action::enroll:!std::strcmp(stored,candidate)?Action::match:Action::reject;
}
}
