#pragma once
#include <cstdint>
#include <cstring>
namespace tapo_session {
// Only connection/identity/credential changes invalidate an authenticated link.
// Store this worker-owned key outside the small protocol task stack.
struct Key {
    char host[16]{},identity[65]{},mac[13]{};
    uint8_t auth[32]{},shadow[20]{};
    bool valid=false,credentials=false,has_shadow=false,has_klap=false;
    template<class C> bool matches(const C &c) const {
        return valid&&credentials==c.credentials&&has_shadow==c.has_shadow&&has_klap==c.has_klap&&
            !std::strcmp(host,c.host)&&!std::strcmp(identity,c.identity)&&!std::strcmp(mac,c.mac)&&
            !std::memcmp(auth,c.auth,sizeof(auth))&&(!has_shadow||!std::memcmp(shadow,c.shadow,sizeof(shadow)));
    }
    template<class C> void capture(const C &c) {
        std::memcpy(host,c.host,sizeof(host));std::memcpy(identity,c.identity,sizeof(identity));std::memcpy(mac,c.mac,sizeof(mac));
        std::memcpy(auth,c.auth,sizeof(auth));std::memcpy(shadow,c.shadow,sizeof(shadow));
        credentials=c.credentials;has_shadow=c.has_shadow;has_klap=c.has_klap;valid=true;
    }
};
inline uint32_t retry_delay(unsigned failures) {
    constexpr uint32_t delay[]={1000,3000,7000,15000,30000};
    return delay[failures<5?failures:4];
}
}
