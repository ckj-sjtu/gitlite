#include"../include/Refs.h"
#include"../include/Utils.h"
#include<cstdio>

Refs::Refs(const std::string& gitliteDir) : dir_{gitliteDir}{}
std::string Refs::currentBranch() const{
    std::string head{Utils::readContentsAsString(Utils::join(dir_, "HEAD"))};
    const std::string prefix{"ref: refs/branches/"};
    if(head.rfind(prefix, 0) == 0){
        std::string name{head.substr(prefix.size())};
        while(!name.empty() && (name.back() == '\n' || name.back() == '\r')){
            name.pop_back();
        }
        return name;
    }
    return "";
}//返回当前branch name
std::string Refs::headCommit() const{
    return getBranch(currentBranch());
}//head->commit sha
void Refs::setHeadBranch(const std::string& name){
    Utils::writeContents(Utils::join(dir_, "HEAD"), "ref: refs/branches/" + name + "\n");
}//change branch
bool Refs::branchExists(const std::string& name) const{
    return Utils::isFile(Utils::join(Utils::join(dir_, "refs/branches"), name));
}
std::string Refs::getBranch(const std::string& name) const{
    std::string path{Utils::join(Utils::join(Utils::join(dir_, "refs"),"branches"), name)};
    if(!Utils::isFile(path)) return "";
    std::string s{Utils::readContentsAsString(path)};
    while(!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s; 
}//name分支最新的commit sha
void Refs::setBranch(const std::string& name, const std::string& sha){
    Utils::writeContents(Utils::join(Utils::join(dir_, "refs/branches"), name), sha + "\n");
}//写refs/branches/<name> 内容为<sha>\n
std::vector<std::string> Refs::listBranches() const {
    return Utils::plainFilenamesIn(Utils::join(dir_, "refs/branches"));
}//列出所有的branches
std::vector<std::string> Refs::listTags() const{
    return Utils::plainFilenamesIn(Utils::join(dir_, "refs/tags"));
}//列出所有的Tags
bool Refs::tagExists(const std::string& name) const{
    return Utils::isFile(Utils::join(Utils::join(dir_, "refs/tags"), name));
}//检测name作为Tag存不存在
std::string Refs::getTag(const std::string& name) const{
    std::string path{Utils::join(Utils::join(dir_, "refs/tags"), name)};
    if(!Utils::isFile(path)) return "";
    std::string s{Utils::readContentsAsString(path)};
    while(!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}//获取tag->commit的sha
void Refs::setTag(const std::string& name, const std::string& sha){
    Utils::writeContents(Utils::join(Utils::join(dir_, "refs/tags"), name), sha + "\n");
}//写tag

void Refs::deleteBranch(const std::string& name){
    std::remove(Utils::join(Utils::join(dir_, "refs/branches"), name).c_str());
}//删掉.gitlite/refs/branches/<name>这个分支