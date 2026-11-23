#include "../include/Index.h"
#include "../include/Utils.h"
#include<sstream>

Index::Index(const std::string& path) : path_(path){}

void Index::load(){
    adds_.clear();
    removes_.clear();
    if(!Utils::exists(path_)) return;
    std::istringstream in(Utils::readContentsAsString(path_));
    std::string line;
    while(std::getline(in, line)){
        if(line.empty()) continue;
        if(line.rfind("add ", 0) == 0){
            std::string rest = line.substr(4);
            if(rest.size() > 41){
                std::string blob = rest.substr(rest.size() - 40);
                std::string name = rest.substr(0, rest.size() - 41);
                adds_[name] = blob;
            }
        }
        else if(line.rfind("remove", 0) == 0){
            removes_.insert(line.substr(7));
        }
    }
}
void Index::save() const{
    std::string s;
    for(const auto& [name, blob] : adds_){
        s += "add " + name + " " + blob + "\n";
    }
    for(const auto& name : removes_){
        s += "remove " + name + "\n"; 
    }
    Utils::writeContents(path_, s);
}

bool Index::hasAdd(const std::string& f) const {return adds_.count(f) > 0;}
bool Index::hasRemove(const std::string& f) const {return removes_.count(f) > 0;}

std::string Index::getAddBlob(const std::string& f) const{
    auto it = adds_.find(f);
    return it == adds_.end() ? "" : it -> second;
}
void Index::setAdd(const std::string& f, const std::string& blob){
    adds_[f] = blob;
    removes_.erase(f);
}
void Index::setRemove(const std::string& f){
    removes_.insert(f);
    adds_.erase(f);
}
void Index::erase(const std::string& f){
    adds_.erase(f);
    removes_.erase(f);
}
void Index::clear(){
    adds_.clear();
    removes_.clear();
}
bool Index::empty() const{
    return adds_.empty() && removes_.empty();
}

