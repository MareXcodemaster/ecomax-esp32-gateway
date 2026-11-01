#include "tpap_crypto.hpp"
#include "mbedtls/ecp.h"
#include "mbedtls/ccm.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"
#include <algorithm>
#include <cstring>

namespace tapo_tpap {
namespace {
void wipe(Bytes &v){if(!v.empty())mbedtls_platform_zeroize(v.data(),v.size());v.clear();}
Bytes bytes(const char *s){return {s,s+strlen(s)};}
Bytes hash(const Bytes &v){Bytes out(32);if(mbedtls_sha256(v.data(),v.size(),out.data(),0))return {};return out;}
Bytes hmac(const Bytes &key,const Bytes &v){
    Bytes out(32);auto *md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(!md||mbedtls_md_hmac(md,key.data(),key.size(),v.data(),v.size(),out.data()))return {};
    return out;
}
Bytes hkdf(const Bytes &input,const Bytes &salt,const char *info,size_t size){
    Bytes prk=hmac(salt.empty()?Bytes(32):salt,input),last,out;
    if(prk.size()!=32)return {};
    for(unsigned block=1;out.size()<size&&block<256;block++){
        Bytes data=last;auto label=bytes(info);data.insert(data.end(),label.begin(),label.end());data.push_back(block);
        wipe(last);last=hmac(prk,data);wipe(data);if(last.size()!=32){wipe(prk);wipe(out);return {};}
        out.insert(out.end(),last.begin(),last.end());
    }
    wipe(prk);wipe(last);out.resize(size);return out;
}
bool pbkdf2(const Bytes &password,const Bytes &salt,unsigned iterations,Bytes &out,Yield yield){
    if(salt.size()<8||salt.size()>64||iterations<1||iterations>100000)return false;
    mbedtls_md_context_t context;mbedtls_md_init(&context);
    const auto *md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    int rc=md?mbedtls_md_setup(&context,md,1):-1;
    if(!rc)rc=mbedtls_md_hmac_starts(&context,password.data(),password.size());
    uint8_t u[32]{},sum[32]{};out.clear();
    for(unsigned block=1;block<=3&&!rc;block++){
        Bytes first=salt;first.insert(first.end(),{0,0,0,static_cast<uint8_t>(block)});
        rc=mbedtls_md_hmac_reset(&context);if(!rc)rc=mbedtls_md_hmac_update(&context,first.data(),first.size());
        if(!rc)rc=mbedtls_md_hmac_finish(&context,u);
        memcpy(sum,u,32);
        for(unsigned round=1;round<iterations&&!rc;round++){
            rc=mbedtls_md_hmac_reset(&context);if(!rc)rc=mbedtls_md_hmac_update(&context,u,32);
            if(!rc)rc=mbedtls_md_hmac_finish(&context,u);
            for(unsigned i=0;i<32;i++)sum[i]^=u[i];
            if(yield&&round%128==0)yield();
        }
        out.insert(out.end(),sum,sum+32);if(yield)yield();
    }
    mbedtls_md_free(&context);mbedtls_platform_zeroize(u,sizeof(u));mbedtls_platform_zeroize(sum,sizeof(sum));
    if(rc){wipe(out);return false;}out.resize(80);return true;
}
struct Mpi {mbedtls_mpi v;Mpi(){mbedtls_mpi_init(&v);}~Mpi(){mbedtls_mpi_free(&v);}};
struct Point {mbedtls_ecp_point v;Point(){mbedtls_ecp_point_init(&v);}~Point(){mbedtls_ecp_point_free(&v);}};
struct Group {mbedtls_ecp_group v;Group(){mbedtls_ecp_group_init(&v);}~Group(){mbedtls_ecp_group_free(&v);}};
Bytes encoded(mbedtls_ecp_group &g,const mbedtls_ecp_point &p){
    Bytes out(65);size_t count=0;
    if(mbedtls_ecp_point_write_binary(&g,&p,MBEDTLS_ECP_PF_UNCOMPRESSED,&count,out.data(),out.size())||count!=65)return {};
    return out;
}
bool point(mbedtls_ecp_group &g,Point &p,const Bytes &input){
    return input.size()==65&&input[0]==4&&!mbedtls_ecp_point_read_binary(&g,&p.v,input.data(),input.size())&&!mbedtls_ecp_check_pubkey(&g,&p.v);
}
bool scalar(mbedtls_ecp_group &g,Mpi &n,const Bytes &input){
    return !input.empty()&&input.size()<=40&&!mbedtls_mpi_read_binary(&n.v,input.data(),input.size())&&
        !mbedtls_mpi_mod_mpi(&n.v,&n.v,&g.N)&&!mbedtls_ecp_check_privkey(&g,&n.v);
}
void transcript(Bytes &out,const Bytes &v){
    uint64_t n=v.size();for(unsigned i=0;i<8;i++)out.push_back((n>>(i*8))&255);
    out.insert(out.end(),v.begin(),v.end());
}
}
Bytes hex(const char *s){
    Bytes out;auto nibble=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};
    size_t n=strlen(s);if(n%2)return {};
    for(size_t i=0;i<n;i+=2){int a=nibble(s[i]),b=nibble(s[i+1]);if(a<0||b<0)return {};out.push_back((a<<4)|b);}return out;
}
bool equal(const Bytes &a,const Bytes &b){if(a.size()!=b.size())return false;unsigned difference=0;for(size_t i=0;i<a.size();i++)difference|=a[i]^b[i];return !difference;}
Proof::~Proof(){wipe(shared);wipe(verifier_key);wipe(confirm);}
bool Proof::verify(const Bytes &value)const{return value.size()==32&&verifier_key.size()==32&&equal(hmac(verifier_key,share),value);}
bool prove(const Bytes &w0_bytes,const Bytes &w1_bytes,const Bytes &x_bytes,const Bytes &context,
           const Bytes &id_a,const Bytes &id_b,const Bytes &device_share,Proof &out,Random random,void *random_context,Yield yield){
    if(!random||context.size()>128||id_a.size()>128||id_b.size()>128)return false;
    Group group;if(mbedtls_ecp_group_load(&group.v,MBEDTLS_ECP_DP_SECP256R1))return false;
    Mpi w0,w1,x,one;
    if(!scalar(group.v,w0,w0_bytes)||!scalar(group.v,w1,w1_bytes)||!scalar(group.v,x,x_bytes)||mbedtls_mpi_lset(&one.v,1))return false;
    Point m,n,y,wm,wn,xg,share,unmasked,z,v;
    if(!point(group.v,m,hex("04886e2f97ace46e55ba9dd7242579f2993b64e16ef3dcab95afd497333d8fa12f5ff355163e43ce224e0b0e65ff02ac8e5c7be09419c785e0ca547d55a12e2d20"))||
       !point(group.v,n,hex("04d8bbd6c639c62937b04d997f38c3770719c629d7014d49a24b4f98baa1292b4907d60aa6bfade45008a636337f5168c64d9bd36034808cd564490b1e656edbe7"))||!point(group.v,y,device_share))return false;
    auto multiply=[&](Point &result,Mpi &k,const mbedtls_ecp_point &p){int rc=mbedtls_ecp_mul(&group.v,&result.v,&k.v,&p,random,random_context);if(yield)yield();return rc==0;};
    // Secret multipliers use the blinded constant-time API. muladd only sees 1.
    if(!multiply(xg,x,group.v.G)||!multiply(wm,w0,m.v)||!multiply(wn,w0,n.v)||
       mbedtls_ecp_muladd(&group.v,&share.v,&one.v,&xg.v,&one.v,&wm.v)||
       mbedtls_mpi_sub_mpi(&wn.v.MBEDTLS_PRIVATE(Y),&group.v.P,&wn.v.MBEDTLS_PRIVATE(Y))||
       mbedtls_ecp_muladd(&group.v,&unmasked.v,&one.v,&y.v,&one.v,&wn.v)||
       mbedtls_ecp_is_zero(&unmasked.v)||!multiply(z,x,unmasked.v)||!multiply(v,w1,unmasked.v)||
       mbedtls_ecp_is_zero(&z.v)||mbedtls_ecp_is_zero(&v.v))return false;
    Bytes w0_fixed(32);if(mbedtls_mpi_write_binary(&w0.v,w0_fixed.data(),w0_fixed.size()))return false;
    auto sx=encoded(group.v,share.v),sy=encoded(group.v,y.v),sm=encoded(group.v,m.v),sn=encoded(group.v,n.v),sz=encoded(group.v,z.v),sv=encoded(group.v,v.v);
    if(sx.empty()||sy.empty()||sm.empty()||sn.empty()||sz.empty()||sv.empty())return false;
    Bytes tt;for(const Bytes *part:std::initializer_list<const Bytes *>{&context,&id_a,&id_b,&sm,&sn,&sx,&sy,&sz,&sv,&w0_fixed})transcript(tt,*part);
    auto main=hash(tt);auto keys=hkdf(main,{},"ConfirmationKeys",64);
    out.shared=hkdf(main,{},"SharedKey",32);wipe(tt);wipe(main);wipe(sz);wipe(sv);wipe(w0_fixed);
    if(keys.size()!=64||out.shared.size()!=32)return false;
    out.share=sx;out.confirm=hmac({keys.begin(),keys.begin()+32},sy);out.verifier_key.assign(keys.begin()+32,keys.end());wipe(keys);
    return out.confirm.size()==32;
}
bool password_proof(const Bytes &shadow,const Bytes &salt,unsigned iterations,const Bytes &user_random,
                    const Bytes &device_random,const Bytes &device_share,Proof &out,Random random,void *random_context,Yield yield){
    if(shadow.size()!=20||user_random.size()!=32||device_random.size()!=32||!random)return false;
    const char *digits="0123456789abcdef";Bytes password;
    for(auto byte:shadow){password.push_back(digits[byte>>4]);password.push_back(digits[byte&15]);}
    Bytes wide;if(!pbkdf2(password,salt,iterations,wide,yield)){wipe(password);return false;}wipe(password);
    Group g;Mpi x;if(mbedtls_ecp_group_load(&g.v,MBEDTLS_ECP_DP_SECP256R1)){wipe(wide);return false;}
    bool generated=false;for(unsigned i=0;i<128;i++)if(!mbedtls_mpi_fill_random(&x.v,32,random,random_context)&&!mbedtls_ecp_check_privkey(&g.v,&x.v)){generated=true;break;}
    Bytes xb(32);if(!generated||mbedtls_mpi_write_binary(&x.v,xb.data(),xb.size())){wipe(wide);return false;}
    Bytes context=bytes("PAKE V1");context.insert(context.end(),user_random.begin(),user_random.end());context.insert(context.end(),device_random.begin(),device_random.end());context=hash(context);
    Bytes w0(wide.begin(),wide.begin()+40),w1(wide.begin()+40,wide.end());wipe(wide);
    bool ok=prove(w0,w1,xb,context,{},{},device_share,out,random,random_context,yield);wipe(w0);wipe(w1);wipe(xb);return ok;
}
void Cipher::reset(){mbedtls_platform_zeroize(key_,sizeof(key_));mbedtls_platform_zeroize(nonce_,sizeof(nonce_));ready_=false;uses_=0;}
bool Cipher::init(const Bytes &shared,uint32_t seq){
    reset();if(shared.size()!=32)return false;
    auto key=hkdf(shared,bytes("tp-kdf-salt-aes128-key"),"tp-kdf-info-aes128-key",16);
    auto nonce=hkdf(shared,bytes("tp-kdf-salt-aes128-iv"),"tp-kdf-info-aes128-iv",12);
    if(key.size()!=16||nonce.size()!=12)return false;
    memcpy(key_,key.data(),16);memcpy(nonce_,nonce.data(),12);wipe(key);wipe(nonce);sequence_=seq;ready_=true;return true;
}
bool Cipher::encrypt(const std::string &message,Bytes &out,uint32_t &seq){
    if(!ready_||message.size()>4096||uses_>=0xffffffffULL)return false;
    seq=sequence_++;uses_++;uint8_t nonce[12];memcpy(nonce,nonce_,12);out.resize(message.size()+20);
    for(unsigned i=0;i<4;i++)nonce[8+i]=out[i]=(seq>>(24-i*8))&255;
    mbedtls_ccm_context c;mbedtls_ccm_init(&c);int rc=mbedtls_ccm_setkey(&c,MBEDTLS_CIPHER_ID_AES,key_,128);
    if(!rc)rc=mbedtls_ccm_encrypt_and_tag(&c,message.size(),nonce,12,nullptr,0,reinterpret_cast<const uint8_t *>(message.data()),out.data()+4,out.data()+4+message.size(),16);
    mbedtls_ccm_free(&c);if(rc){wipe(out);return false;}return true;
}
bool Cipher::decrypt(uint32_t seq,const Bytes &body,std::string &out)const{
    out.clear();if(!ready_||body.size()<20||body.size()>8192)return false;
    uint8_t nonce[12];memcpy(nonce,nonce_,12);
    for(unsigned i=0;i<4;i++){nonce[8+i]=(seq>>(24-i*8))&255;if(body[i]!=nonce[8+i])return false;}
    out.resize(body.size()-20);mbedtls_ccm_context c;mbedtls_ccm_init(&c);int rc=mbedtls_ccm_setkey(&c,MBEDTLS_CIPHER_ID_AES,key_,128);
    if(!rc)rc=mbedtls_ccm_auth_decrypt(&c,out.size(),nonce,12,nullptr,0,body.data()+4,reinterpret_cast<uint8_t *>(out.data()),body.data()+body.size()-16,16);
    mbedtls_ccm_free(&c);if(rc){if(!out.empty())mbedtls_platform_zeroize(out.data(),out.size());out.clear();return false;}return true;
}
bool self_test(Random random,void *random_context,Yield yield){
    Proof p;if(!prove(hex("bb8e1bbcf3c48f62c08db243652ae55d3e5586053fca77102994f23ad95491b3"),
        hex("7e945f34d78785b8a3ef44d0df5a1a97d6b3b460409a345ca7830387a74b1dba"),
        hex("d1232c8e8693d02368976c174e2088851b8365d0d79a9eee709c6a05a2fad539"),
        bytes("SPAKE2+-P256-SHA256-HKDF-SHA256-HMAC-SHA256 Test Vectors"),bytes("client"),bytes("server"),
        hex("04c0f65da0d11927bdf5d560c69e1d7d939a05b0e88291887d679fcadea75810fb5cc1ca7494db39e82ff2f50665255d76173e09986ab46742c798a9a68437b048"),p,random,random_context,yield))return false;
    if(p.share!=hex("04ef3bd051bf78a2234ec0df197f7828060fe9856503579bb1733009042c15c0c1de127727f418b5966afadfdd95a6e4591d171056b333dab97a79c7193e341727")||
       p.confirm!=hex("926cc713504b9b4d76c9162ded04b5493e89109f6d89462cd33adc46fda27527")||
       p.shared!=hex("0c5f8ccd1413423a54f6c1fb26ff01534a87f893779c6e68666d772bfd91f3e7")||
       !p.verify(hex("9747bcc4f8fe9f63defee53ac9b07876d907d55047e6ff2def2e7529089d3e68"))||p.verify(p.confirm))return false;
    Bytes derived;if(!pbkdf2(bytes("credential"),Bytes(16),3,derived,yield)||derived!=hex("3b427a675ef8f5adf88da62ed1d5fa9c21f4301979722b80fde7a735a1b2eebfb403f9c208b169ab4080ff14479b34da6c172ecffad23ea8bf218d8fd6cc174c9efc7256b7636e1516eff5af3a01accb"))return false;
    Bytes shared(32);for(unsigned i=0;i<32;i++)shared[i]=i;
    Cipher c;if(!c.init(shared,0x01020304))return false;Bytes body;uint32_t seq;std::string plain;
    if(!c.encrypt("{\"method\":\"get_device_info\"}",body,seq)||body!=hex("01020304c0331c1c36a5df4ce0c68e5a27b2667ebf842dc994a3fd24b9e860a60fab76d4b66b9b669d95cca10c012f13")||
       !c.decrypt(seq,body,plain)||plain!="{\"method\":\"get_device_info\"}"||c.decrypt(seq+1,body,plain))return false;
    body.back()^=1;return !c.decrypt(seq,body,plain)&&plain.empty();
}
}
