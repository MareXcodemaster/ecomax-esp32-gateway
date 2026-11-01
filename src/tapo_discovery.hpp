#pragma once
#include <string>
#include <vector>
namespace tapo_discovery {
struct Device {std::string host,mac,model,protocol;};
inline bool supported_model(const std::string &value){
    auto suffix=value.find('(');if(suffix!=std::string::npos&&(value.back()!=')'||suffix+2>=value.size()))return false;
    auto model=value.substr(0,suffix);
    return model=="P100"||model=="P105"||model=="P110"||model=="P110M"||model=="P115";
}
inline bool metered_model(const std::string &value){auto model=value.substr(0,value.find('('));return supported_model(value)&&(model=="P110"||model=="P110M"||model=="P115");}
inline std::string normalize_mac(const std::string &value){
    std::string out;
    for(unsigned char c:value){if(c==':'||c=='-')continue;if(c>='a'&&c<='f')c-=32;if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))return {};out+=c;}
    return out.size()==12?out:std::string{};
}
std::vector<Device> scan();
}
