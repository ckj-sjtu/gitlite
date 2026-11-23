#include "../include/Checkout.h"
#include<cstdio>
#include "../include/Commit.h"
#include "../include/ObjectStore.h"
#include "../include/Refs.h"
#include "../include/Utils.h"
#include "../include/WorkTree.h"
#include "../include/Index.h"

Checkout::Checkout(Refs& refs, ObjectStore& store, Index& idx, const WorkTree& wt):refs_{refs}, store_{store}, idx_{idx}, wt_{wt}, rules_{".gitliteignore"}{};
bool Checkout::untrackedInTheWay(const Commit& cur, const Commit& target) const{
    for(const auto& kv : target.snapshot){
        const std::string& p{kv.first};
        if(!wt_.exists(p)) continue;
        if(cur.snapshot.count(p) > 0) continue;
        if(idx_.hasAdd(p)) continue;
        if(rules_.isIgnored(p)) continue;
        Utils::message("There is an untracked file in the way; delete it, or add and commit it first.");
        return true;
    }
    return false;
}//检查target和工作区中重复的文件

bool Checkout::toSnapshot(const Commit& target){
    Commit cur{Commit::deserialize(store_.readObject(refs_.headCommit()))};
    if(untrackedInTheWay(cur, target)){
        return false;
    }
    //delete
    for(const auto& kv : cur.snapshot){
        if(target.snapshot.count(kv.first) > 0) continue;
        if(wt_.exists(kv.first)) std::remove(kv.first.c_str());  
    }
    //写入target.snapshot里的所有文件
    for(const auto& kv : target.snapshot){
        Utils::writeContents(kv.first, store_.readObject(kv.second));
    }
    return true;
}//把工作区整体切换到目标提交的snapshot

bool Checkout::fileTo(const Commit& commit, const std::string& f){
    auto it{commit.snapshot.find(f)};
    if(it == commit.snapshot.end()){
        Utils::message("File does not exist in that commit.");
        return false;
    }
    Utils::writeContents(f, store_.readObject(it->second));
    return true;
}//用commit里的版本，覆盖f这个文件