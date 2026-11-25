#include "../include/ObjectStore.h"
#include "../include/Utils.h"

ObjectStore::ObjectStore(const std::string& dir) : dir_{dir}{
    Utils::createDirectories(dir_);
}
bool ObjectStore::exists(const std::string& sha) const{
    return Utils::isFile(Utils::join(dir_, sha));
}//objects/<sha> exists?
std::string ObjectStore::writeBlob(const std::string& content){
    std::string sha{Utils::sha1(content)};
    std::string path{Utils::join(dir_, sha)};
    if(!Utils::exists(path)){
        Utils::writeContents(path, content);
    }
    return sha;
}//blob write objects/<sha>
std::string ObjectStore::writeCommit(const std::string& serialized){
    std::string sha{Utils::sha1(serialized)};
    std::string path{Utils::join(dir_, sha)};
    if(!Utils::exists(path)){
        Utils::writeContents(path, serialized);
    }
    return sha;
}//commit write objects/<sha>
std::string ObjectStore::readObject(const std::string& sha) const{
    return Utils::readContentsAsString(Utils::join(dir_, sha));
}//返回objects/<sha>的内容
std::vector<std::string> ObjectStore::listObjects() const{
    return Utils::plainFilenamesIn(dir_);
}

void ObjectStore::importFrom(const ObjectStore& src){
    std::vector<std::string> objects{src.listObjects()};
    for(auto object : objects){
        if(exists(object)) continue;
        Utils::writeContents(Utils::join(dir_, object), src.readObject(object));
    }
}//从src把仓库搬过来