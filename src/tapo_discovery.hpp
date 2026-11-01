#pragma once
#include <string>
#include <vector>
namespace tapo_discovery {
struct Device {std::string host,mac,model,protocol;};
inline std::string normalize_mac(const std::string &value){
    std::string out;
    for(unsigned char c:value){if(c==':'||c=='-')continue;if(c>='a'&&c<='f')c-=32;if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))return {};out+=c;}
    return out.size()==12?out:std::string{};
}
std::vector<Device> scan();
}
