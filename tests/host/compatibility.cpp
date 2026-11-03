#include "compatibility_report.hpp"
#include "identity_guard.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
using Bytes=std::vector<uint8_t>;
Bytes packet(unsigned type,const Bytes &payload,unsigned source=69){
 size_t n=payload.size()+10;Bytes b={0x68,uint8_t(n),uint8_t(n>>8),86,uint8_t(source),48,5,uint8_t(type)};
 b.insert(b.end(),payload.begin(),payload.end());uint8_t sum=0;for(auto v:b)sum^=v;b.push_back(sum);b.push_back(0x16);return b;
}
cJSON *safe(unsigned type,const Bytes &payload){auto b=packet(type,payload);return compatibility_report::frame(b.data(),b.size());}
std::string dump(cJSON *p){char *s=cJSON_PrintUnformatted(p);std::string out=s?s:"";cJSON_free(s);cJSON_Delete(p);return out;}
}
void compatibility_tests(){
 const std::string model="ecoMAX OTHER";Bytes product={0,42,0,3,11,22,33,2,0,3,0,uint8_t(model.size())};product.insert(product.end(),model.begin(),model.end());
 auto b=packet(185,product);auto *raw=protocol::describe(b.data(),b.size());
 std::string uid=compatibility_report::get(compatibility_report::get(raw,"data"),"uid")->valuestring;cJSON_Delete(raw);assert(!uid.empty());
 auto *out=safe(185,product);assert(out);std::string text=dump(out);
 assert(text.find(model)!=std::string::npos&&text.find(uid)==std::string::npos&&text.find("\"uid\"")==std::string::npos&&text.find("\"hex\"")==std::string::npos);
 for(size_t n=0;n<product.size();++n){text=dump(safe(185,Bytes(product.begin(),product.begin()+n)));assert(text.find("\"hex\"")==std::string::npos&&text.find(uid)==std::string::npos);}
 Bytes modules={255,255,5,42,0,11,22,33,9,0,12,0,34,0,69};text=dump(safe(192,modules));
 assert(text.find("9.12.34")!=std::string::npos&&text.find("processor_signature")==std::string::npos&&text.find("device_id_raw")==std::string::npos&&text.find("\"hex\"")==std::string::npos);
 text=dump(safe(214,{4,1,0,40,35,65}));assert(text.find("\"hex\"")!=std::string::npos);
 for(unsigned cmd:{1,2,5,6,7,8,255})assert(!safe(214,{uint8_t(cmd),1,0}));
 for(unsigned type:{0,1,8,86,112,189,240,250})assert(!safe(type,{1,2,3}));
 for(size_t n=0;n<b.size();++n)assert(!compatibility_report::frame(b.data(),n));
 auto broken=b;broken[10]^=1;assert(!compatibility_report::frame(broken.data(),broken.size()));
 broken=packet(185,product,86);assert(!compatibility_report::frame(broken.data(),broken.size()));
 auto *mixed=cJSON_Parse("{\"number\":42,\"bool\":false,\"secret\":\"SECRET\",\"nested\":{\"text\":\"SECRET\",\"value\":23},\"list\":[1,2,\"SECRET\"]}");
 text=dump(compatibility_report::numeric(mixed));cJSON_Delete(mixed);assert(text.find("SECRET")==std::string::npos&&text.find("42")!=std::string::npos);
 assert(!strcmp(identity_guard::rejection("", "GOOD123",false),"unsupported_model"));
 assert(!strcmp(identity_guard::rejection("OTHER123", "GOOD123",true),"identity_mismatch"));
 assert(!strcmp(identity_guard::rejection("", "bad identity",true),"invalid_identity"));
 assert(!identity_guard::rejection("", "GOOD123",true));
 assert(!identity_guard::rejection("GOOD123", "GOOD123",true));
 assert(sizeof(compatibility_report::queries)/sizeof(compatibility_report::queries[0])==14);
 puts("PASS compatibility: actual other model retained; UID/module identifiers stripped; malformed/unknown frames bounded; identity guards preserved");
}
