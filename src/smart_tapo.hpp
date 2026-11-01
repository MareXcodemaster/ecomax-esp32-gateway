#pragma once
#include "tpap_crypto.hpp"
#include "third_party/esp_tapo/tapo_client.h"
namespace smart_tapo {
class Transport:public esphome::tapo::TapoTransport {
public:
    virtual Response exchange(const std::string &path,const std::string &body,const char *content_type)=0;
};
class Client {
    Transport *transport_=nullptr;
    esphome::tapo::TapoClient legacy_;
    tapo_tpap::Cipher cipher_;
    tapo_tpap::Random random_;
    void *random_context_;
    tapo_tpap::Yield yield_;
    uint64_t (*clock_)();
    uint64_t expires_=0;
    std::string path_,error_;
    bool ready_=false,tpap_=false,rejected_=false,detected_=false;
public:
    Client(tapo_tpap::Random random,void *context,tapo_tpap::Yield yield,uint64_t (*clock)()):random_(random),random_context_(context),yield_(yield),clock_(clock){}
    void set_transport(Transport *transport){transport_=transport;legacy_.set_transport(transport);}
    bool handshake(const tapo_tpap::Bytes &klap_auth,const tapo_tpap::Bytes &shadow,bool has_klap);
    bool is_ready();
    void reset();
    std::string send_message(const std::string &message);
    const std::string &error()const{return error_;}
    bool credentials_rejected()const{return rejected_;}
    const char *protocol()const{return detected_?(tpap_?"TPAP":"KLAP"):"";}
};
}
