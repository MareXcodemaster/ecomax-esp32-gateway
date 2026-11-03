#include "remote_menu_language.hpp"
#include "model810.hpp"
#include "model810_fixtures.hpp"
#include "protocol.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
namespace {
using Bytes=std::vector<uint8_t>;
void text(Bytes &b,const char *s){while(*s)b.push_back(*s++);b.push_back(0);}
}
void remote_menu_language_tests(){
 using namespace remote_menu_language;
 auto *decoded=protocol::describe(model810_metadata_frame,sizeof(model810_metadata_frame));
 assert(model810::metadata_match(model810::get(model810::get(decoded,"data"),"parameters")));cJSON_Delete(decoded);

 Bytes languages={1,2,1};text(languages,"pl");text(languages,"Polski");languages.insert(languages.end(),{1,2,3,4});
 text(languages,"en");text(languages,"English");languages.insert(languages.end(),{5,6,7,8});
 assert(language(languages.data(),languages.size())==0);
 for(size_t n=0;n<languages.size();++n)assert(language(languages.data(),n)==-1);
 languages[2]=2;assert(language(languages.data(),languages.size())==-1);languages[2]=1;
 const uint8_t names[]={2,0,8,10};Bytes data={2,0,2,10};text(data,"Test %%");text(data,"Próba °C");
 auto *p=page(names,4,data.data(),data.size());assert(p&&cJSON_GetArraySize(p)==2);
 assert(!strcmp(cJSON_GetArrayItem(p,0)->valuestring,"Test %"));cJSON_Delete(p);
 for(size_t n=0;n<data.size();++n)assert(!page(names,4,data.data(),n));
 auto bad=data;bad[1]=1;assert(!page(names,4,bad.data(),bad.size()));bad=data;bad[3]=11;assert(!page(names,4,bad.data(),bad.size()));
 bad=data;bad.push_back(0);assert(!page(names,4,bad.data(),bad.size()));
 bad={2,0,1,10,0xc0,0xaf,0};assert(!page(names,4,bad.data(),bad.size()));
 bad={2,0,0,10};p=page(names,4,bad.data(),bad.size());assert(p&&cJSON_GetArraySize(p)==0);cJSON_Delete(p);
 assert(!utf8(std::string("\xed\xa0\x80",3)));assert(!utf8(std::string("\xf4\x90\x80\x80",4)));
 const uint8_t enums[]={7,0,1,0};Bytes e={7,0,1,0,2,3};text(e,"Nie");text(e,"Tak");
 p=page(enums,4,e.data(),e.size());assert(p&&cJSON_GetArraySize(p)==1);
 auto *entry=cJSON_GetArrayItem(p,0);assert(cJSON_GetObjectItemCaseSensitive(entry,"first")->valueint==3);assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(entry,"values"))==2);cJSON_Delete(p);
 e[4]=65;assert(!page(enums,4,e.data(),e.size()));
 const uint8_t units[]={6,0};Bytes u={6,0,2};text(u,"");text(u,"°C");p=page(units,2,u.data(),u.size());assert(p);cJSON_Delete(p);
 for(unsigned cmd:{0,3,4,5,9,10,11,12,13,14,15,255}){uint8_t q[]={uint8_t(cmd),0,255,0};assert(!allowed(q,4));}
 assert(allowed(names,4)&&allowed(units,2));assert(!allowed(nullptr,1));
 for(unsigned cmd:{2,7,8}){uint8_t all[]={uint8_t(cmd),0,255,0};assert(!allowed(all,4));}
 uint8_t too_many[]={2,0,8,10};Bytes many={2,0,9,10};assert(!matches(too_many,4,many.data(),many.size()));
 uint8_t last[]={2,0,8,168};Bytes last_page={2,0,1,168};text(last_page,"Ostatnia");p=page(last,4,last_page.data(),last_page.size());assert(p&&cJSON_GetArraySize(p)==1);cJSON_Delete(p);

 assert(model810::matches("ecoMAX810P-L TOUCH","10.40.186.R5","10.14.50"));
 assert(!model810::matches("ecoMAX810P-L TOUCH","10.40.186.R4","10.14.50"));
 assert(!model810::matches("ecoMAX810P-L","10.40.186.R5","10.14.50"));
 auto *metadata=cJSON_CreateArray();
 for(unsigned i=0;i<model810::parameter_count;++i){auto *m=cJSON_CreateObject();cJSON_AddItemToArray(metadata,m);auto &x=model810::metadata[i];
  cJSON_AddNumberToObject(m,"index",i);cJSON_AddNumberToObject(m,"unit",x.unit);cJSON_AddNumberToObject(m,"mult",x.mult);cJSON_AddNumberToObject(m,"offset",x.offset);}
 assert(model810::metadata_match(metadata));cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(metadata,168),"offset"),999);assert(!model810::metadata_match(metadata));cJSON_Delete(metadata);
 auto *versions=cJSON_CreateObject(),*commands=cJSON_CreateArray();cJSON_AddNumberToObject(versions,"command",0);cJSON_AddNumberToObject(versions,"major",1);cJSON_AddNumberToObject(versions,"minor",0);cJSON_AddItemToObject(versions,"commands",commands);
 for(auto &x:model810::versions){auto *v=cJSON_CreateObject();cJSON_AddItemToArray(commands,v);cJSON_AddNumberToObject(v,"command",x[0]);cJSON_AddNumberToObject(v,"version",x[1]);}
 assert(model810::version_match(versions));cJSON_AddItemToArray(commands,cJSON_Duplicate(cJSON_GetArrayItem(commands,0),true));assert(!model810::version_match(versions));cJSON_Delete(versions);
 puts("PASS remote menu: Polish text, language versions, pagination correlation, enum offsets, truncation/UTF-8/bounds rejection, exact 810 profile");
}
