// Test-only bridge: exercise the production encoder from browser integration
// tests and local payload measurements. Never communicates with a device.
#include "panel_sync.hpp"
#include <iostream>
#include <iterator>
#include <string>
int main(int argc,char **argv){
    std::string input((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
    auto *source=cJSON_Parse(input.c_str());if(!source)return 2;
    auto *value=panel_sync::snapshot(source,argc>1?argv[1]:"",argc>2?argv[2]:"test-boot");
    cJSON_Delete(source);if(!value)return 3;
    auto *json=cJSON_PrintUnformatted(value);cJSON_Delete(value);if(!json)return 4;
    std::cout<<json;cJSON_free(json);return 0;
}
