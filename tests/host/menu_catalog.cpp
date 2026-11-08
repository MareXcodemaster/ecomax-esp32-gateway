#include "menu_catalog_reader.hpp"
#include "model860p3_fixtures.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
namespace {
using Bytes=std::vector<uint8_t>;
using menu_catalog::Reader;
void text(Bytes &p,const std::string &s){p.insert(p.end(),s.begin(),s.end());p.push_back(0);}
Bytes frame(const Bytes &p){
 size_t n=p.size()+10;Bytes f={0x68,uint8_t(n),uint8_t(n>>8),86,80,48,5,214};
 f.insert(f.end(),p.begin(),p.end());uint8_t checksum=0;for(auto b:f)checksum^=b;
 f.push_back(checksum);f.push_back(0x16);return f;
}
template<size_t N> Bytes fixture(const uint8_t (&f)[N]){return Bytes(f,f+N);}
Bytes response(const uint8_t *q,unsigned names=198){
 if(q[0]==0)return fixture(model860p3_versions_frame);
 if(q[0]==3)return fixture(model860p3_metadata_frame);
 if(q[0]==9)return fixture(model860p3_structure_frame);
 Bytes p;
 if(q[0]==1){p={1,1,0};text(p,"pl");text(p,"Polski");p.insert(p.end(),{1,2,3,4});}
 else {
  unsigned total=q[0]==2?names:q[0]==6?32:q[0]==7?31:72;
  unsigned start=q[0]==6?0:q[3],count=q[0]==6?total:std::min(unsigned(q[2]),total>start?total-start:0U);
  p={q[0],q[1],uint8_t(count)};if(q[0]!=6)p.push_back(start);
  for(unsigned i=0;i<count;++i){
   if(q[0]==7){p.push_back(2);p.push_back(0);text(p,"Nie");text(p,"Tak");}
   else text(p,"Test "+std::to_string(start+i));
  }
 }
 return frame(p);
}
void advance(Reader &r,unsigned stop){
 for(unsigned n=0;r.stage()<stop&&n<160;++n){uint8_t q[4]{};auto size=r.query(q);auto f=response(q);assert(size&&r.accept(q,size,f.data(),f.size()));}
 assert(r.stage()==stop);
}
void set(cJSON *o,const char *k,double n){cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(o,k),n);}
}
void menu_catalog_tests(){
 using namespace controller_profile;
 assert(family(0,"ecoMAX860P3-O")&&firmware("16.33.91.P1")&&firmware("16.11.10"));
 Reader r;r.start();
 // Feed the real schema and synthetic text pages in separate invocations. A
 // lost response must not discard already received names or change the offset.
 advance(r,3);
 for(unsigned i=0;i<3;++i){uint8_t q[4]{};auto n=r.query(q);auto f=response(q);assert(r.accept(q,n,f.data(),f.size()));}
 assert(r.names_count()==24&&!r.complete());
 uint8_t lost[4]{},again[4]{};auto n=r.query(lost);assert(r.missed());auto m=r.query(again);
 assert(n==m&&!memcmp(lost,again,n)&&r.names_count()==24);
 auto f=response(again);assert(r.accept(again,m,f.data(),f.size()));
 unsigned batches=0;while(!r.complete()){
  for(unsigned i=0;i<4&&!r.complete();++i){uint8_t q[4]{};auto size=r.query(q);auto reply=response(q);bool accepted=size&&r.accept(q,size,reply.data(),reply.size());if(!accepted)fprintf(stderr,"stage=%u query=%u offset=%u error=%s\n",r.stage(),q[0],q[3],r.error());assert(accepted);}
  ++batches;assert(batches<40);
 }
 assert(batches>10&&r.requests()<100&&r.names_count()==198);
 assert(cJSON_GetArraySize(r.parameters())==198&&cJSON_GetArraySize(r.structure())==274);
 assert(structure_valid(r.structure(),r.table("rmCatsNames"),198));
 // The complete report-derived tree was rejected by the former 256-node cap.
 // Preserve access information; do not flatten locks or protected categories.
 unsigned protected_nodes=0,settings=0;const cJSON *row;
 cJSON_ArrayForEach(row,r.structure()){if(number(get(row,"pass_index"))>0)++protected_nodes;if(number(get(row,"type"))==1)++settings;}
 assert(protected_nodes==6&&settings==106);
 auto *tree=cJSON_Duplicate(r.structure(),true);auto *first=cJSON_GetArrayItem(tree,0);
 set(first,"pass_index",8);assert(!structure_valid(tree,r.table("rmCatsNames"),198));set(first,"pass_index",0);
 set(first,"type",1.5);assert(!structure_valid(tree,r.table("rmCatsNames"),198));cJSON_Delete(tree);
 tree=cJSON_CreateArray();auto *names=cJSON_CreateArray();cJSON_AddItemToArray(names,cJSON_CreateString("Root"));
 for(unsigned i=0;i<257;++i){auto *entry=cJSON_Parse(R"({"type":0,"index":0,"pass_index":0,"lock":false})");cJSON_AddItemToArray(tree,entry);}
 assert(!structure_valid(tree,names,198));cJSON_DeleteItemFromArray(tree,256);assert(structure_valid(tree,names,198));
 cJSON_Delete(tree);cJSON_Delete(names);
 r.reset();assert(!r.active()&&!r.complete()&&!r.requests());

 // Changed versions, language and metadata invalidate all staged data before
 // publication, even if a large catalogue was otherwise received successfully.
 for(unsigned stage:{8,9,10}){
  r.start();advance(r,stage);uint8_t q[4]{};auto size=r.query(q);auto reply=response(q);
  Bytes payload(reply.begin()+8,reply.end()-2);
  if(stage==8)payload[8]^=1;else if(stage==9)payload.back()^=1;else payload.back()^=1;
  reply=frame(payload);assert(!r.accept(q,size,reply.data(),reply.size())&&!r.complete()&&r.error());
 }
 r.start();advance(r,3);uint8_t q[4]{};auto size=r.query(q);f=response(q);
 auto corrupt=f;corrupt[corrupt.size()-2]^=1;assert(!r.accept(q,size,corrupt.data(),corrupt.size()));
 r.start();advance(r,3);size=r.query(q);f=response(q);
 Bytes shifted(f.begin()+8,f.end()-2);shifted[3]=1;f=frame(shifted);assert(!r.accept(q,size,f.data(),f.size()));
 r.start();for(unsigned i=0;i<160;++i){assert(r.query(q));}assert(!r.query(q)&&r.error());
 r.start();assert(r.missed()&&r.missed()&&!r.missed()&&r.error());
 r.start();advance(r,3);
 bool ok=true;for(unsigned i=0;i<100&&ok&&r.stage()<7;++i){size=r.query(q);f=response(q,197);ok=r.accept(q,size,f.data(),f.size());}
 assert(!ok&&r.error()&&!r.complete());
 puts("PASS ecoMAX860P3-O: 198 parameters/274 nodes/106 menu settings, resumable pages, bounded retries, changed versions/language/metadata and access guards");
}
