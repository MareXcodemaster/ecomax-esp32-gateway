#include "tpap_crypto.hpp"
#include "smart_tapo.hpp"
#include <cassert>
#include <cstdio>
#include <random>
int random_bytes(void *,unsigned char *out,size_t size){
    static std::mt19937 engine(42);for(size_t i=0;i<size;i++)out[i]=engine();return 0;
}
void esp_fill_random(void *buffer,size_t size){random_bytes(nullptr,static_cast<unsigned char *>(buffer),size);}
uint64_t clock_ms(){return 1000;}
struct FakeTransport:smart_tapo::Transport {
    int calls=0;
    std::string reply=R"({"error_code":0,"result":{"tpap":{"tls":0,"port":80,"pake":[2]}}})";
    Response exchange(const std::string &,const std::string &,const char *)override{calls++;return {200,reply,{}};}
    Response post(const std::string &,const std::string &,const std::string &)override{assert(false);return {};}
    void reset()override{}
};
int main(){
    using namespace tapo_tpap;
    assert(self_test(random_bytes,nullptr));
    Proof p;Bytes share(65,1);share[0]=4;
    assert(!prove(Bytes{1},Bytes{1},Bytes{1},{},{},{},share,p,random_bytes,nullptr));
    assert(!password_proof(Bytes(20),Bytes(16),100001,Bytes(32),Bytes(32),share,p,random_bytes,nullptr));
    assert(!password_proof(Bytes(19),Bytes(16),1,Bytes(32),Bytes(32),share,p,random_bytes,nullptr));
    Cipher cipher;uint32_t seq=0;Bytes body;std::string plain;
    assert(!cipher.encrypt("uninitialized",body,seq));
    assert(!cipher.init(Bytes(31),0));assert(cipher.init(Bytes(32),0x80000000));
    assert(cipher.encrypt("signed sequence",body,seq)&&seq==0x80000000);
    assert(cipher.decrypt(seq,body,plain)&&plain=="signed sequence");
    body.resize(19);assert(!cipher.decrypt(seq,body,plain));
    cipher.reset();assert(!cipher.encrypt("reset",body,seq));
    FakeTransport transport;smart_tapo::Client client(random_bytes,nullptr,nullptr,clock_ms);client.set_transport(&transport);
    assert(!client.handshake(Bytes(32),{},true));assert(transport.calls==1&&!client.credentials_rejected());
    assert(client.error().find("ponownie")!=std::string::npos);
    transport.reply=R"({"error_code":-2101})";
    assert(!client.handshake(Bytes(32),Bytes(20),true));assert(client.credentials_rejected());
    transport.reply=R"({"error_code":0,"result":{"tpap":{"tls":1,"port":443,"pake":[2]}}})";
    assert(!client.handshake(Bytes(32),Bytes(20),true));assert(!client.credentials_rejected());
    assert(transport.calls==3);
    puts("PASS TPAP: RFC 9383 proof, PBKDF2, known AES-CCM vector, mutual confirmation, invalid point/parameters/tag/sequence and session reset");
}
