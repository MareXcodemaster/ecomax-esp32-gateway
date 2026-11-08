#pragma once
#include "controller_profile.hpp"
#include "remote_menu_language.hpp"
#include "protocol.hpp"

// Read-only, resumable catalogue acquisition. The owner checks device identity
// and yields between bounded batches. Nothing is published until all tables,
// permissions, versions and metadata have passed the final consistency checks.
namespace menu_catalog {
class Reader {
 using J=cJSON;
 J *staged_=nullptr;
 unsigned stage_=0,pages_=0,requests_=0,failures_=0;
 size_t bytes_=0;
 int language_=-1;
 std::string languages_;
 const char *error_=nullptr;
 static constexpr const char *tables[]={"rmParamsNames","rmParamsUnitsNames","rmParamsEnums","rmCatsNames"};
 static constexpr uint8_t commands[]={2,6,7,8};
 static const J *get(const J *o,const char *k){return controller_profile::get(o,k);}
 void put(const char *key,J *v){cJSON_DeleteItemFromObjectCaseSensitive(staged_,key);cJSON_AddItemToObject(staged_,key,v);}
 bool fail(const char *e){error_=e;return false;}
 bool check_tables(){
  if(cJSON_GetArraySize(table("rmParamsNames"))!=cJSON_GetArraySize(parameters()))return fail("Liczba nazw nie odpowiada metadanym nastaw sterownika");
  const J *row;cJSON_ArrayForEach(row,parameters()){
   int unit=controller_profile::number(get(row,"unit")),offset=controller_profile::number(get(row,"offset"));
   if(unit==31){if(!cJSON_GetArrayItem(table("rmParamsEnums"),offset))return fail("Brak opcji nastawy w katalogu kotla");}
   else if(!cJSON_GetArrayItem(table("rmParamsUnitsNames"),unit))return fail("Brak jednostki nastawy w katalogu kotla");
  }
  return true;
 }
public:
 Reader()=default;
 Reader(const Reader&)=delete;
 Reader& operator=(const Reader&)=delete;
 ~Reader(){reset();}
 void reset(){cJSON_Delete(staged_);staged_=nullptr;stage_=pages_=requests_=failures_=0;bytes_=0;language_=-1;languages_.clear();error_=nullptr;}
 void start(){reset();staged_=cJSON_CreateObject();if(!staged_)error_="Brak pamieci katalogu RS485";}
 bool active()const{return staged_!=nullptr;}
 bool complete()const{return active()&&!error_&&stage_==11;}
 const char *error()const{return error_;}
 unsigned requests()const{return requests_;}
 unsigned stage()const{return stage_;}
 unsigned names_count()const{return cJSON_GetArraySize(table("rmParamsNames"));}
 const J *table(const char *key)const{return get(staged_,key);}
 const J *parameters()const{return table("parameters");}
 const J *structure()const{return table("structure");}
 const J *versions()const{return table("versions");}
 // The same page is requested again after a timeout; already validated pages
 // are retained. Limit consecutive failures and all requests, not elapsed time
 // of one worker invocation.
 bool missed(){return ++failures_<3?true:fail("Brak odpowiedzi katalogu RS485 po trzech probach; ponowne pobranie za minute");}
 size_t query(uint8_t (&q)[4]){
  if(!active()||error_||complete())return 0;
  if(++requests_>160){fail("Przekroczono limit zapytan katalogu RS485");return 0;}
  if(stage_>=3&&stage_<=6){
   q[0]=commands[stage_-3];q[1]=language_;q[2]=q[0]==7?1:8;q[3]=cJSON_GetArraySize(table(tables[stage_-3]));
   return q[0]==6?2:4;
  }
  q[0]=stage_==0||stage_==8?0:stage_==1||stage_==10?3:stage_==7?9:1;
  return 1;
 }
 bool accept(const uint8_t *q,size_t qn,const uint8_t *frame,size_t n){
  if(!active()||error_||complete()||!frame||n<10||n>1000||!q||!qn||frame[7]!=214||frame[8]!=q[0])return fail("Niepoprawna odpowiedz katalogu RS485");
  struct Owned{J *p;~Owned(){cJSON_Delete(p);}} parsed{protocol::describe(frame,n)};
  auto *data=get(parsed.p,"data");
  const char *state=controller_profile::text(get(parsed.p,"state"));
  if(std::strcmp(state,"decoded")&&std::strcmp(state,"reference_only"))return fail("Niepoprawna ramka katalogu RS485");
  failures_=0;
  if(stage_>=3&&stage_<=6){
   if((bytes_+=n)>65536||++pages_>64)return fail("Katalog nazw przekroczyl limit pamieci lub stron");
   Owned page{remote_menu_language::page(q,qn,frame+8,n-10)};
   if(!page.p)return fail("Niepoprawna lub przesunieta strona nazw RS485");
   const char *key=tables[stage_-3];auto *values=const_cast<J *>(table(key));
   if(!values){values=cJSON_CreateArray();put(key,values);}
   unsigned count=cJSON_GetArraySize(page.p),total=cJSON_GetArraySize(values)+count;
   if(total>256)return fail("Niepoprawna liczba nazw RS485");
   while(cJSON_GetArraySize(page.p))cJSON_AddItemToArray(values,cJSON_DetachItemFromArray(page.p,0));
   if(q[0]==6||!count||total>=255){++stage_;pages_=0;}
   return stage_!=7||check_tables();
  }
  if(stage_==2||stage_==9){
   std::string payload(reinterpret_cast<const char *>(frame+8),n-10);
   if(stage_==2){language_=remote_menu_language::language(frame+8,n-10);if(language_<0)return fail("Niepoprawna lista jezykow menu RS485");languages_=payload;}
   else if(payload!=languages_)return fail("Jezyk lub wersja tekstow menu zmienily sie podczas pobierania");
  }else{
   if(std::strcmp(controller_profile::text(get(parsed.p,"state")),"decoded"))return fail("Niepoprawny format katalogu RS485");
   if(stage_==0){if(!controller_profile::versions_valid(data))return fail("Nieobslugiwany lub niepelny protokol katalogu menu RS485");put("versions",cJSON_Duplicate(data,true));}
   else if(stage_==1){if(!controller_profile::metadata_valid(get(data,"parameters")))return fail("Niepoprawne metadane menu sterownika RS485");put("parameters",cJSON_Duplicate(get(data,"parameters"),true));}
   else if(stage_==7){if(!controller_profile::structure_valid(get(data,"structure"),table("rmCatsNames"),cJSON_GetArraySize(parameters())))return fail("Niepoprawna struktura lub uprawnienia menu sterownika");put("structure",cJSON_Duplicate(get(data,"structure"),true));}
   else if(stage_==8){if(!controller_profile::same_versions(versions(),data))return fail("Katalog zmienil sie podczas pobierania");}
   else if(stage_==10&&!controller_profile::same_metadata(parameters(),get(data,"parameters")))return fail("Metadane zmienily sie podczas odczytu katalogu");
  }
  ++stage_;return true;
 }
};
}
