#include "../include/Revision.h"
#include "../include/Refs.h"
#include "../include/ObjectStore.h"

namespace{
    bool isHex(const std::string& s){
        if(s.empty()) return false;
        for(char c : s){
            bool ok{(c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')};
            if(!ok) return false;
        }
        return true;
    }//判断s是不是纯小写十六进制
}

std::string Revision::resolve(Refs& refs, ObjectStore& store, const std::string& spec){
    std::string byTag{refs.getTag(spec)};
    if(!byTag.empty()) return byTag;

    if(!isHex(spec) || spec.size() > 40) return "";

    std::string best;
    std::vector<std::string> objs{store.listObjects()};
    for(const std::string& name : objs){
        if(name.size() != 40) continue;
        if(name.rfind(spec, 0) != 0) continue;
        std::string content = store.readObject(name);
        if(content.rfind("commit\n", 0) != 0) continue;
        if(best.empty() || name < best) best = name;
    }
    return best;
}//把用户输入的字符串变成commit的sha