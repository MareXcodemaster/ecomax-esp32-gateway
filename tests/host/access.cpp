#include "device_access.hpp"
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <string>

namespace {
std::string stored,pending;
bool commit_failure=false;
unsigned writes=0;
}
esp_err_t nvs_open(const char* name,int,nvs_handle_t* handle){
    assert(!std::strcmp(name,"device_access"));*handle=1;return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t,const char* key,void* value,size_t* length){
    assert(!std::strcmp(key,"credentials"));
    if(stored.empty())return ESP_ERR_NVS_NOT_FOUND;
    if(*length<stored.size())return ESP_FAIL;
    std::memcpy(value,stored.data(),stored.size());*length=stored.size();return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t,const char* key,const void* value,size_t length){
    assert(!std::strcmp(key,"credentials"));++writes;
    pending.assign(static_cast<const char*>(value),length);return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t){
    if(commit_failure)return ESP_FAIL;
    stored=pending;return ESP_OK;
}
void nvs_close(nvs_handle_t){}
void access_tests(){
    assert(device_access::init()==ESP_OK);
    assert(!std::strcmp(device_access::password(),"admin"));
    assert(!std::strcmp(device_access::setup_password(),"piecwifi"));
    unsigned initial=writes;
    assert(device_access::init()==ESP_OK&&writes==initial);
    assert(device_access::configure("wrong","newpassword")==ESP_ERR_INVALID_ARG);
    assert(device_access::configure("admin","1234")==ESP_ERR_INVALID_ARG);
    assert(device_access::configure("admin","newpassword","admin")==ESP_ERR_INVALID_ARG);
    assert(writes==initial);
    assert(device_access::configure("admin","my-password","my-wifi-pass")==ESP_OK);
    assert(!std::strcmp(device_access::password(),"admin"));
    assert(device_access::init()==ESP_OK);
    assert(!std::strcmp(device_access::password(),"my-password"));
    assert(!std::strcmp(device_access::setup_password(),"my-wifi-pass"));
    assert(device_access::configure("my-password","admin")==ESP_OK);
    assert(device_access::init()==ESP_OK);
    assert(!std::strcmp(device_access::password(),"admin"));
    assert(!std::strcmp(device_access::setup_password(),"my-wifi-pass"));
    commit_failure=true;
    assert(device_access::configure("admin","newpassword")==ESP_FAIL);
    assert(!std::strcmp(device_access::password(),"admin"));
    assert(device_access::init()==ESP_OK&&!std::strcmp(device_access::password(),"admin"));
    stored="corrupt";
    assert(device_access::init()==ESP_ERR_INVALID_STATE);
    assert(!*device_access::password());
    stored.clear();
    assert(device_access::init()==ESP_FAIL&&!*device_access::password());
    commit_failure=false;
    assert(device_access::init()==ESP_OK&&!std::strcmp(device_access::password(),"admin"));
    assert(device_access::valid_password("admin",5));
    assert(!device_access::valid_password("admin",8));
    assert(!device_access::valid_password("abc\nxyz",5));
    assert(!device_access::valid_password(std::string(49,'a').c_str(),5));
}
