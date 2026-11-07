#include "panel_sync.hpp"
#include <cassert>
#include <string>
#include <cstdio>
namespace {
struct Owned {cJSON *p;~Owned(){cJSON_Delete(p);}};
std::string token(cJSON *v){return panel_sync::get(panel_sync::get(v,"sync"),"catalog")->valuestring;}
cJSON *restore(cJSON *live,cJSON *catalog){
    auto *out=cJSON_Duplicate(live,true);cJSON_DeleteItemFromObjectCaseSensitive(out,"sync");cJSON_DeleteItemFromObjectCaseSensitive(out,"catalog");
    const cJSON *field=nullptr;cJSON_ArrayForEach(field,catalog){
        if(strcmp(field->string,"menu")&&strcmp(field->string,"targets")){cJSON_AddItemToObject(out,field->string,cJSON_Duplicate(field,true));continue;}
        auto *rows=panel_sync::get(out,field->string);cJSON *row=nullptr;
        cJSON_ArrayForEach(row,rows){
            const cJSON *base=nullptr,*candidate=nullptr;
            if(!strcmp(field->string,"targets"))base=panel_sync::get(field,row->string);
            else cJSON_ArrayForEach(candidate,field)if(cJSON_Compare(panel_sync::get(candidate,"index"),panel_sync::get(row,"index"),true))base=candidate;
            assert(base);const cJSON *m=nullptr;cJSON_ArrayForEach(m,base){
                if(!strcmp(m->string,"metadata")){const cJSON *v=nullptr;cJSON_ArrayForEach(v,m)cJSON_AddItemToObject(panel_sync::get(row,"metadata"),v->string,cJSON_Duplicate(v,true));}
                else if(!panel_sync::get(row,m->string))cJSON_AddItemToObject(row,m->string,cJSON_Duplicate(m,true));
            }
        }
    }
    return out;
}
}
void panel_sync_tests(){
    Owned source{cJSON_Parse(R"({"system":{"controllerID":"test"},"write_transport":"rs485","fresh":true,"curr":{"tempCWU":41},"names":{"1281":{"name":"CWU"}},"units":["C"],"menu":[{"index":0,"name":"Test","metadata":{"value":41,"minv":10,"maxv":80,"edit":true},"read_available":true,"write_enabled":true,"enum_text":null},{"index":"STER_TEMP_DAY_1","metadata":{"value":21},"cached":false}],"targets":{"1281":{"id":"1281","name":"CWU","min":10,"max":80,"step":1,"schema":"original","before":41,"effective":36,"write_ready":true}}})")};assert(source.p);
    Owned first{panel_sync::snapshot(source.p,"","boot-1")};assert(first.p);auto revision=token(first.p);
    auto *catalog=panel_sync::get(first.p,"catalog");assert(catalog);
    Owned original{restore(first.p,catalog)};assert(cJSON_Compare(original.p,source.p,true));
    auto *menu=cJSON_GetArrayItem(panel_sync::get(source.p,"menu"),0);
    cJSON_SetNumberValue(panel_sync::get(panel_sync::get(menu,"metadata"),"value"),42);
    cJSON_SetNumberValue(panel_sync::get(panel_sync::get(panel_sync::get(source.p,"targets"),"1281"),"before"),42);
    Owned second{panel_sync::snapshot(source.p,revision.c_str(),"boot-1")};assert(second.p&&token(second.p)==revision&&!panel_sync::get(second.p,"catalog"));
    Owned changed{restore(second.p,catalog)};assert(cJSON_Compare(changed.p,source.p,true));
    cJSON_SetNumberValue(panel_sync::get(panel_sync::get(menu,"metadata"),"maxv"),75);
    Owned range{panel_sync::snapshot(source.p,revision.c_str(),"boot-1")};assert(range.p&&token(range.p)!=revision&&panel_sync::get(range.p,"catalog"));
    Owned restarted{panel_sync::snapshot(source.p,token(range.p).c_str(),"boot-2")};assert(restarted.p&&panel_sync::get(restarted.p,"catalog"));
    cJSON_DeleteItemFromObjectCaseSensitive(panel_sync::get(source.p,"targets"),"1281");
    Owned removed{panel_sync::snapshot(source.p,token(restarted.p).c_str(),"boot-2")};assert(removed.p&&panel_sync::get(removed.p,"catalog"));
    Owned actual{restore(removed.p,panel_sync::get(removed.p,"catalog"))};assert(cJSON_Compare(actual.p,source.p,true));
    // Fault every allocation in a small response. A failed snapshot must not
    // return a partial catalogue that would leave stale write capabilities.
    static unsigned allocation=0,fail_at=0;
    cJSON_Hooks hooks{[](size_t n)->void*{return ++allocation==fail_at?nullptr:std::malloc(n);},std::free};
    for(fail_at=1;fail_at<230;++fail_at){allocation=0;cJSON_InitHooks(&hooks);Owned v{panel_sync::snapshot(source.p,"","boot-3")};cJSON_InitHooks(nullptr);if(v.p){Owned restored{restore(v.p,panel_sync::get(v.p,"catalog"))};assert(cJSON_Compare(restored.p,source.p,true));}}
    cJSON_InitHooks(nullptr);
    std::puts("Panel compact snapshots: complete reconstruction, external values, range/identity/restart/removal and allocation failures PASS");
}
