#include "smart_tapo.hpp"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include <cmath>
#include <cstring>
#include <ctime>
#include <limits>
namespace smart_tapo {
namespace {
using tapo_tpap::Bytes;
const cJSON *get(const cJSON *v,const char *name){return cJSON_GetObjectItemCaseSensitive(v,name);}
std::string text(const cJSON *v){return cJSON_IsString(v)?v->valuestring:"";}
bool integer(const cJSON *v,double low,double high){return cJSON_IsNumber(v)&&std::isfinite(v->valuedouble)&&v->valuedouble==std::floor(v->valuedouble)&&v->valuedouble>=low&&v->valuedouble<=high;}
struct Json {cJSON *v=nullptr;Json(cJSON *value=nullptr):v(value){}~Json(){cJSON_Delete(v);}Json(const Json&)=delete;};
std::string base64(const Bytes &v){
    size_t count=0;std::string out((v.size()+2)/3*4+1,'\0');
    if(mbedtls_base64_encode(reinterpret_cast<unsigned char *>(out.data()),out.size(),&count,v.data(),v.size()))return {};
    out.resize(count);return out;
}
Bytes decoded(const cJSON *v,size_t minimum,size_t maximum){
    auto value=text(v);if(value.empty()||value.size()>(maximum+2)/3*4)return {};
    Bytes out(maximum);size_t count=0;if(mbedtls_base64_decode(out.data(),out.size(),&count,reinterpret_cast<const unsigned char *>(value.data()),value.size())||count<minimum||count>maximum)return {};
    out.resize(count);return out;
}
std::string encoded(cJSON *value){char *raw=cJSON_PrintUnformatted(value);if(!raw)return {};std::string out(raw);cJSON_free(raw);return out;}
}
void Client::reset(){ready_=false;expires_=0;path_.clear();cipher_.reset();legacy_.reset();}
bool Client::is_ready(){if(ready_&&clock_&&clock_()>=expires_)reset();return ready_;}
bool Client::handshake(const Bytes &auth,const Bytes &shadow,bool has_klap){
    reset();error_.clear();rejected_=false;
    if(!transport_||!random_||!clock_){error_="Brak klienta Tapo";return false;}
    auto discovery=transport_->exchange("/",R"({"method":"login","params":{"sub_method":"discover"}})","application/json");
    if(discovery.status<=0){error_="Brak odpowiedzi P110 w sieci lokalnej";return false;}
    Json discover(cJSON_ParseWithLength(discovery.body.data(),discovery.body.size()));
    auto *info=get(get(discover.v,"result"),"tpap");
    if(!cJSON_IsObject(info)){
        if(integer(get(discover.v,"error_code"),-2101,-2101)){rejected_=true;error_="Tapo zablokowalo logowanie; odczekaj i sprawdz haslo";return false;}
        tpap_=false;detected_=true;
        if(!has_klap){error_="P110 wymaga KLAP: wpisz takze e-mail Tapo";return false;}
        if(!legacy_.handshake(auth)){rejected_=legacy_.get_error_message().find("invalid credentials")!=std::string::npos;
            error_=rejected_?"KLAP: odrzucono dane konta; ponowne proby wstrzymane":"Brak sesji KLAP; sprawdz dostepnosc P110";return false;}
        ready_=true;expires_=clock_()+900000;return true;
    }
    tpap_=true;detected_=true;
    if(discovery.status!=200||!integer(get(discover.v,"error_code"),0,0)){error_="Nieprawidlowa odpowiedz wykrywania TPAP";return false;}
    bool account=false;const auto *pake=get(info,"pake");for(int i=0;i<cJSON_GetArraySize(pake);i++)if(integer(cJSON_GetArrayItem(pake,i),2,2))account=true;
    const auto *port=get(info,"port");
    if(!integer(get(info,"tls"),0,0)||(port&&!integer(port,80,80))||!account){error_="Nieobslugiwany wariant TPAP (wymagany userpw, HTTP 80)";return false;}
    if(shadow.size()!=20){error_="Wpisz ponownie haslo Tapo, aby uruchomic TPAP";return false;}
    auto login=[&](cJSON *params,Json &response)->bool{
        Json request(cJSON_CreateObject());if(!request.v){cJSON_Delete(params);error_="Brak pamieci na logowanie Tapo";return false;}
        cJSON_AddStringToObject(request.v,"method","login");cJSON_AddItemToObject(request.v,"params",params);
        auto body=encoded(request.v);if(body.empty()){error_="Brak pamieci na logowanie Tapo";return false;}
        auto answer=transport_->exchange("/",body,"application/json");
        if(answer.status!=200){error_="Brak odpowiedzi logowania TPAP";return false;}
        response.v=cJSON_ParseWithLength(answer.body.data(),answer.body.size());auto *code=get(response.v,"error_code");
        if(!integer(code,0,0)){
            int n=cJSON_IsNumber(code)?code->valueint:0;
            rejected_=n==-2203||n==-2101||n==9999;
            error_=rejected_?"TPAP: haslo odrzucone lub blokada logowania; proby wstrzymane":"TPAP: odrzucono uzgadnianie sesji";return false;
        }
        if(!cJSON_IsObject(get(response.v,"result"))){error_="Niepelna odpowiedz logowania TPAP";return false;}return true;
    };
    Bytes user_random(32);if(random_(random_context_,user_random.data(),user_random.size())){error_="Brak losowosci dla TPAP";return false;}
    auto *params=cJSON_CreateObject();if(!params){error_="Brak pamieci na TPAP";return false;}
    cJSON_AddStringToObject(params,"sub_method","pake_register");cJSON_AddStringToObject(params,"username","21232f297a57a5a743894a0e4a801fc3");
    cJSON_AddStringToObject(params,"user_random",base64(user_random).c_str());
    auto *suites=cJSON_AddArrayToObject(params,"cipher_suites");cJSON_AddItemToArray(suites,cJSON_CreateNumber(1));
    auto *encryption=cJSON_AddArrayToObject(params,"encryption");cJSON_AddItemToArray(encryption,cJSON_CreateString("aes_128_ccm"));
    cJSON_AddStringToObject(params,"passcode_type","userpw");Json registration;
    if(!login(params,registration))return false;
    auto *r=get(registration.v,"result"),*extra=get(r,"extra_crypt");auto *iterations=get(r,"iterations");
    auto salt=decoded(get(r,"dev_salt"),8,64),remote=decoded(get(r,"dev_random"),32,32),share=decoded(get(r,"dev_share"),65,65);
    if(!integer(iterations,1,100000)||!integer(get(r,"cipher_suites"),1,1)||text(get(r,"encryption"))!="aes_128_ccm"||
       text(get(extra,"type"))!="password_shadow"||!integer(get(get(extra,"params"),"passwd_id"),2,2)||salt.empty()||remote.empty()||share.empty()){
        error_="Nieobslugiwane parametry kryptografii TPAP";return false;
    }
    tapo_tpap::Proof proof;
    if(!tapo_tpap::password_proof(shadow,salt,iterations->valueint,user_random,remote,share,proof,random_,random_context_,yield_)){
        error_="Nie przygotowano dowodu TPAP; sprawdz pamiec i klucz urzadzenia";return false;
    }
    params=cJSON_CreateObject();if(!params){error_="Brak pamieci na TPAP";return false;}
    cJSON_AddStringToObject(params,"sub_method","pake_share");cJSON_AddStringToObject(params,"user_share",base64(proof.share).c_str());
    cJSON_AddStringToObject(params,"user_confirm",base64(proof.confirm).c_str());Json confirmed;
    if(!login(params,confirmed))return false;
    auto *result=get(confirmed.v,"result");auto token=text(get(result,"stok"));auto *sequence=get(result,"start_seq");
    if(!proof.verify(decoded(get(result,"dev_confirm"),32,32))){rejected_=true;error_="TPAP: P110 nie potwierdzilo hasla; proby wstrzymane";return false;}
    if(token.empty()||token.size()>128||token.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-")!=std::string::npos||
       !integer(sequence,std::numeric_limits<int32_t>::min(),std::numeric_limits<int32_t>::max())){error_="Nieprawidlowa sesja TPAP";return false;}
    if(!cipher_.init(proof.shared,static_cast<uint32_t>(sequence->valueint))){error_="Nie przygotowano szyfrowania TPAP";return false;}
    path_="/stok="+token+"/ds";ready_=true;expires_=clock_()+900000;return true;
}
std::string Client::send_message(const std::string &message){
    if(!is_ready())return {};
    if(!tpap_){auto result=legacy_.send_message(message);if(result.empty()){reset();error_="Utracono sesje KLAP";}return result;}
    Json request(cJSON_ParseWithLength(message.data(),message.size()));
    if(!cJSON_IsObject(request.v)){error_="Nieprawidlowe polecenie TPAP";return {};}
    if(!get(request.v,"requestTimeMils"))cJSON_AddNumberToObject(request.v,"requestTimeMils",time(nullptr)*1000.0);
    auto serialized=encoded(request.v);Bytes body;uint32_t sequence;
    if(serialized.empty()||!cipher_.encrypt(serialized,body,sequence)){reset();error_="Brak gotowej sesji TPAP";return {};}
    auto reply=transport_->exchange(path_,std::string(body.begin(),body.end()),"application/octet-stream");
    std::string result;
    if(reply.status!=200||!cipher_.decrypt(sequence,{reply.body.begin(),reply.body.end()},result)){
        reset();error_="Brak zgodnej szyfrowanej odpowiedzi TPAP";return {};
    }
    return result;
}
}
