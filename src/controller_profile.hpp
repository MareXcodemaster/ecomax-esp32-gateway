#pragma once
#include "cJSON.h"
#include <cmath>
#include <cstring>
#include <initializer_list>

// Standard ecoMAX-P protocol capabilities. Device-specific indices never follow
// from a model name: menu writes need the controller's own complete catalogue.
namespace controller_profile {
inline const cJSON *get(const cJSON *p,const char *k){return cJSON_GetObjectItemCaseSensitive(p,k);}
inline double number(const cJSON *p){return cJSON_IsNumber(p)?p->valuedouble:NAN;}
inline const char *text(const cJSON *p){return cJSON_IsString(p)?p->valuestring:"";}
inline bool integer(double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high&&std::floor(n)==n;}
inline bool family(unsigned type,const char *model){
 if(type!=0||!model||std::strlen(model)>96)return false;
 const char *p=!std::strncmp(model,"ecoMAX",6)?model+6:!std::strncmp(model,"EM",2)?model+2:nullptr;
 if(!p)return false;
 while(*p==' ')++p;
 if(*p<'0'||*p>'9')return false;
 for(const char *s=p;*s;++s)if(static_cast<unsigned char>(*s)<32||static_cast<unsigned char>(*s)>126)return false;
 return true;
}
inline bool firmware(const char *v){return v&&*v&&std::strlen(v)<=64&&std::strcmp(v,"unknown")&&*v>='0'&&*v<='9';}
inline bool reference(const char *m,const char *a,const char *p){
 return m&&a&&p&&!std::strcmp(m,"ecoMAX860P-LZ TOUCH")&&!std::strcmp(a,"8.41.38.L0")&&!std::strcmp(p,"8.30.28");
}
inline bool modules_match(const cJSON *sys,const cJSON *sensor){
 auto *mods=get(sensor,"modules");auto *a=text(get(mods,"module_a")),*p=text(get(mods,"panel"));
 return firmware(a)&&firmware(p)&&!std::strcmp(a,text(get(sys,"moduleASoftVer")))&&!std::strcmp(p,text(get(sys,"modulePanelSoftVer")));
}
constexpr unsigned catalog_commands[]={1,2,3,6,7,8,9};
inline int version(const cJSON *p,unsigned command){
 int found=-1;const cJSON *row;
 cJSON_ArrayForEach(row,get(p,"commands"))if(number(get(row,"command"))==command){
  double v=number(get(row,"version"));if(found>=0||!integer(v,0,65535))return -1;found=int(v);
 }return found;
}
inline bool versions_valid(const cJSON *p){
 if(number(get(p,"command"))!=0||number(get(p,"major"))!=1||number(get(p,"minor"))!=0||!cJSON_IsArray(get(p,"commands")))return false;
 for(auto command:catalog_commands)if(version(p,command)<0)return false;
 return true;
}
inline bool same_versions(const cJSON *a,const cJSON *b){
 if(!versions_valid(a)||!versions_valid(b))return false;
 for(auto command:catalog_commands)if(version(a,command)!=version(b,command))return false;
 return true;
}
inline bool metadata_valid(const cJSON *rows){
 int n=cJSON_GetArraySize(rows);if(!cJSON_IsArray(rows)||n<1||n>255)return false;
 for(int i=0;i<n;++i){auto *r=cJSON_GetArrayItem(rows,i);double u=number(get(r,"unit")),m=number(get(r,"mult")),o=number(get(r,"offset"));
  if(number(get(r,"index"))!=i||!integer(u,0,31)||!std::isfinite(m)||!integer(o,-128,127)||(u==31?(m!=0||o<0):(m<=0||m>10000)))return false;
 }return true;
}
inline bool same_metadata(const cJSON *a,const cJSON *b){
 if(!metadata_valid(a)||!metadata_valid(b)||cJSON_GetArraySize(a)!=cJSON_GetArraySize(b))return false;
 for(int i=0;i<cJSON_GetArraySize(a);++i)for(auto key:{"unit","mult","offset"})
  if(number(get(cJSON_GetArrayItem(a,i),key))!=number(get(cJSON_GetArrayItem(b,i),key)))return false;
 return true;
}
// START/STOP is the documented common 59 / 187 command, independent of the
// remote-menu indices and manufacturer firmware version. Keep live safeguards.
inline const char *control_guard(const cJSON *sensor,bool direct,bool identity,bool enabled,bool start){
 if(!direct)return "START/STOP wymaga trybu RS485 bezposrednio";
 if(!enabled)return "Nadawanie RS485 jest wylaczone";
 if(!identity)return "Oczekiwanie na identyfikacje sterownika ecoMAX po RS485";
 double age=number(get(sensor,"age_ms"));
 if(!cJSON_IsTrue(get(sensor,"fresh"))||!integer(age,0,5000))return "Brak aktualnego odczytu stanu kotla po RS485";
 if(!firmware(text(get(get(sensor,"modules"),"module_a"))))return "Brak odczytu wersji modulu A po RS485";
 auto *values=get(sensor,"sensors");double mode=number(get(values,"state"));
 if(!integer(mode,0,23)||(mode>11&&mode!=12&&mode!=23))return "Nierozpoznany stan kotla";
 if(start&&(mode!=0||number(get(values,"pending_alerts"))!=0||!cJSON_IsFalse(get(get(sensor,"outputs"),"alarm"))))return "START wymaga wylaczonego kotla i braku zgloszonych alarmow";
 return nullptr;
}
}
