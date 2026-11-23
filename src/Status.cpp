#include "../include/Status.h"
#include<iostream>
#include<set>
#include<string>

#include "../include/Commit.h"
#include "../include/IgnoreRules.h"
#include "../include/Index.h"
#include "../include/ObjectStore.h"
#include "../include/Refs.h"
#include "../include/WorkTree.h"

Status::Status(Refs& refs, ObjectStore& store, Index& idx, const WorkTree& wt){
    current_ = refs.currentBranch();//master
    branches_ = refs.listBranches();//所有分支的名字

    Commit head{Commit::deserialize(store.readObject(refs.headCommit()))};
    
    for(const auto& kv : idx.adds()) staged_.push_back(kv.first); 
    for(const auto& r : idx.removes()) removed_.push_back(r);

    std::set<std::string> paths;
    for(const auto& kv : head.snapshot) paths.insert(kv.first);
    for(const auto& kv : idx.adds()) paths.insert(kv.first);
    for(const std::string& p : paths){
        if(idx.hasRemove(p)) continue;
        const std::string base{idx.hasAdd(p) ? idx.getAddBlob(p) : head.snapshot.at(p)};

        if(!wt.exists(p)){
            modified_.push_back(p + " (deleted)");
        }
        else if(wt.sha1Of(p) != base){
            modified_.push_back(p + " (modified)");
        }
    }

    IgnoreRules rules(".gitliteignore");
    for(const std::string& f : wt.listFiles()){
        if(idx.hasAdd(f)) continue;
        bool inHead{head.snapshot.count(f) > 0};
        bool removedStaged{idx.hasRemove(f)};
        if(inHead && !removedStaged) continue;
        if(!(inHead && removedStaged) && rules.isIgnored(f)) continue;
        untracked_.push_back(f);
    }
}
void Status::print() const{
    std::cout << "=== Branches ===\n";
    for(const std::string& b : branches_){
        if(b == current_) std::cout << '*';
        std::cout << b << "\n";
    }
    std::cout << "\n";

    std::cout << "=== Staged Files ===\n";
    for(const std::string& s : staged_) std::cout << s << "\n";
    std::cout << "\n";

    std::cout << "=== Removed Files ===\n";
    for(const std::string& s : removed_) std::cout << s << "\n";
    std::cout << "\n";

    std::cout << "=== Modifications Not Staged For Commit ===\n";
    for(const std::string& s :modified_) std::cout << s << "\n";
    std::cout << "\n";

    std::cout << "=== Untracked Files ===\n";
    for(const std::string& s : untracked_) std::cout << s << "\n";
    std::cout << "\n";
}//输出status