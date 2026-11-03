#pragma once
#include "cJSON.h"
#include <cstdint>
#include <cstring>
#include <string>

// Independently implemented bounded reader for the remote-menu text tables.
// Passwords (10), writes (5) and arbitrary caller-selected commands are excluded.
namespace remote_menu_language {
inline bool allowed(const uint8_t *p,size_t n){
 return p&&((n==1&&p[0]==1)||(n==2&&p[0]==6&&p[1]<32)||
  (n==4&&(p[0]==2||p[0]==7||p[0]==8)&&p[1]<32&&p[2]==255));
}
inline bool matches(const uint8_t *q,size_t qn,const uint8_t *p,size_t n){
 if(!allowed(q,qn)||!p||!n||p[0]!=q[0])return false;
 if(qn==1)return n>=3;
 if(n<3||p[1]!=q[1])return false;
 return qn==2||(n>=4&&p[3]==q[3]&&unsigned(p[2])+p[3]<=256);
}
inline bool utf8(const std::string &s){
 for(size_t i=0;i<s.size();){unsigned c=uint8_t(s[i++]),v=c,extra=0,min=0;
  if(c<128){if(c<32&&c!=9&&c!=10&&c!=13)return false;continue;}
  if(c>=194&&c<=223){extra=1;v=c&31;min=128;}
  else if(c>=224&&c<=239){extra=2;v=c&15;min=2048;}
  else if(c>=240&&c<=244){extra=3;v=c&7;min=65536;}else return false;
  if(extra>s.size()-i)return false;
  while(extra--){unsigned b=uint8_t(s[i++]);if((b&192)!=128)return false;v=(v<<6)|(b&63);}
  if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return false;
 }return true;
}
struct Reader {
 const uint8_t *p;size_t n,at=0;bool ok=true;
 unsigned byte(){if(at>=n){ok=false;return 0;}return p[at++];}
 std::string text(){std::string s;bool end=false;while(ok&&s.size()<=384){unsigned c=byte();if(!ok)break;if(!c){end=true;break;}s+=char(c);}
  if(!end||!utf8(s)){ok=false;return {};}
  size_t pos=0;while((pos=s.find("%%",pos))!=std::string::npos)s.erase(pos,1);
  return s;
 }
};
inline int language(const uint8_t *p,size_t n){
 if(!p||n<3||n>990||p[0]!=1)return -1;
 Reader r{p,n};r.byte();unsigned count=r.byte(),def=r.byte();
 if(!count||count>32||def>=count)return -1;
 int polish=-1;
 for(unsigned i=0;i<count&&r.ok;++i){auto code=r.text();r.text();for(unsigned j=0;j<4;++j)r.byte();if(code=="pl")polish=i;}
 return r.ok&&r.at==n?(polish<0?int(def):polish):-1;
}
inline cJSON *page(const uint8_t *q,size_t qn,const uint8_t *p,size_t n){
 if(qn==1||!matches(q,qn,p,n)||n>990)return nullptr;
 Reader r{p,n};r.byte();r.byte();unsigned count=r.byte();if(qn==4)r.byte();
 if(q[0]==6&&count>32)return nullptr;
 auto *out=cJSON_CreateArray();
 for(unsigned i=0;i<count&&r.ok;++i){
  if(q[0]!=7){auto text=r.text();cJSON_AddItemToArray(out,cJSON_CreateString(text.c_str()));}
  else {unsigned size=r.byte(),first=r.byte();if(size>64||size+first>256){r.ok=false;break;}
   auto *entry=cJSON_CreateObject(),*values=cJSON_CreateArray();cJSON_AddItemToArray(out,entry);
   cJSON_AddNumberToObject(entry,"first",first);cJSON_AddItemToObject(entry,"values",values);
   for(unsigned j=0;j<size&&r.ok;++j){auto text=r.text();cJSON_AddItemToArray(values,cJSON_CreateString(text.c_str()));}
  }
 }
 if(!r.ok||r.at!=n){cJSON_Delete(out);return nullptr;}return out;
}
}
