#include "../include/WorkTree.h"
#include "../include/Utils.h"

WorkTree::WorkTree(const std::string& dir) : dir_{dir}{}
std::vector<std::string> WorkTree::listFiles() const{
    return Utils::plainFilenamesIn(dir_);
}//列出工作区的所有文件
bool WorkTree::exists(const std::string& f) const{
    return Utils::isFile(Utils::join(dir_, f));
}//判断这个文件在不在
std::string WorkTree::sha1Of(const std::string& f) const{
    return Utils::sha1(Utils::readContentsAsString(Utils::join(dir_, f)));
}//返回这个文件内容对应的sha1