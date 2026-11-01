#pragma once
#include <cstdint>
#include <string>
#include <vector>

// TPAP wire rules adapted from MIT mihai-dinculescu/tapo; see THIRD_PARTY.md.
namespace tapo_tpap {
using Bytes=std::vector<uint8_t>;
using Random=int (*)(void *,unsigned char *,size_t);
using Yield=void (*)();
Bytes hex(const char *text);
bool equal(const Bytes &a,const Bytes &b);
struct Proof {
    Bytes share,confirm,shared,verifier_key;
    ~Proof();
    bool verify(const Bytes &device_confirm)const;
};
bool prove(const Bytes &w0,const Bytes &w1,const Bytes &x,const Bytes &context,
           const Bytes &id_a,const Bytes &id_b,const Bytes &device_share,
           Proof &out,Random random,void *random_context,Yield yield=nullptr);
bool password_proof(const Bytes &password_shadow,const Bytes &salt,unsigned iterations,
                    const Bytes &user_random,const Bytes &device_random,const Bytes &device_share,
                    Proof &out,Random random,void *random_context,Yield yield=nullptr);
class Cipher {
    uint8_t key_[16]{},nonce_[12]{};
    uint32_t sequence_=0;
    uint64_t uses_=0;
    bool ready_=false;
public:
    ~Cipher(){reset();}
    void reset();
    bool init(const Bytes &shared,uint32_t sequence);
    bool encrypt(const std::string &message,Bytes &out,uint32_t &sequence);
    bool decrypt(uint32_t sequence,const Bytes &body,std::string &out)const;
};
bool self_test(Random random,void *random_context,Yield yield=nullptr);
}
