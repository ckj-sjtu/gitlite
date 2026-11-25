#include "../include/Remote.h"

#include<cstdio>
#include<set>
#include<string>
#include<vector>

#include "../include/Commit.h"
#include "../include/ObjectStore.h"
#include "../include/Refs.h"
#include "../include/Utils.h"

Remote::Remote(const std::string& gitliteDir):dir_{gitliteDir}{
    Utils::createDirectories(Utils::join(dir_, "remotes"));
}//创建remotes/目录

std::string Remote::path(const std::string& name) const{
    std::string p{Utils::join(Utils::join(dir_, "remotes"), name)};
    if(!Utils::isFile(p)) return "";
    std::string s{Utils::readContentsAsString(p)};
    while(!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}//输出name对应的path
bool Remote::exists(const std::string& name) const{
    std::string p{Utils::join(Utils::join(dir_, "remotes"), name)};
    if(Utils::isFile(p)) return true;
    return false;
}//检查远程仓库name是否存在
void Remote::add(const std::string& name, const std::string& dir){
    Utils::writeContents(Utils::join(Utils::join(dir_, "remotes"), name), dir + "\n");
}//给name文件写入路径dir
void Remote::remove(const std::string& name){
    std::remove(Utils::join(Utils::join(dir_, "remotes"), name).c_str());
}//删去name这个文件

bool Remote::resolve(const std::string& name, std::string& dir) const{
    dir = path(name);
    if(dir.empty() || !Utils::isDirectory(dir)){
        Utils::message("Remote directory not found.");
        return false;
    }
    return true;
}//往dir里写入name仓库的路径

bool Remote::push(const std::string& name, const std::string& branch){
    std::string rpath;
    if(!resolve(name, rpath)) return false;

    Refs localRefs{dir_};
    Refs remoteRefs{rpath};
    ObjectStore localStore{Utils::join(dir_, "objects")};
    ObjectStore remoteStore{Utils::join(rpath, "objects")};

    const std::string localHead{localRefs.headCommit()};
    const std::string remoteHead{remoteRefs.getBranch(branch)};

    if(!remoteHead.empty()){
        std::set<std::string> ancestors;
        std::vector<std::string> stack{localHead};
        while(!stack.empty()){
            std::string u{stack.back()};
            stack.pop_back();
            if(!ancestors.insert(u).second) continue;
            Commit cm{Commit::deserialize(localStore.readObject(u))};
            std::vector<std::string> parents{cm.parents};
            for(const auto& p : parents){
                stack.push_back(p);
            }
        } 
        if(ancestors.count(remoteHead) == 0){
            Utils::message("Please pull down remote changes before pushing.");
            return false;
        }
    }

    remoteStore.importFrom(localStore);
    remoteRefs.setBranch(branch, localHead);
    return true;
}//把本地仓库推送到远程name仓库的分支branch上

bool Remote::fetch(const std::string& name, const std::string& branch){
    std::string rpath;
    if(!resolve(name, rpath)) return false;

    Refs localRefs{dir_};
    Refs remoteRefs{rpath};
    ObjectStore localStore{Utils::join(dir_, "objects")};
    ObjectStore remoteStore{Utils::join(rpath, "objects")};

    const std::string& localHead(localRefs.headCommit());
    const std::string& remoteHead{remoteRefs.getBranch(branch)};
    if(remoteHead.empty()){
        Utils::message("That remote does not have that branch.");
        return false;
    }

    localStore.importFrom(remoteStore);
    localRefs.setBranch(name + "/" + branch, remoteHead);
    return true;
}//把远程name仓库的branch分支移到本地