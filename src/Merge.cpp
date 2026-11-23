#include "../include/Merge.h"

#include<cstdio>
#include<ctime>
#include<map>
#include<queue>
#include<set>
#include<string>
#include<vector>

#include "../include/Checkout.h"
#include "../include/Commit.h"
#include "../include/Index.h"
#include "../include/ObjectStore.h"
#include "../include/Refs.h"
#include "../include/Utils.h"
#include "../include/WorkTree.h"

namespace{

const std::string* lookup(const std::map<std::string, std::string>& m, const std::string& key){
    auto it{m.find(key)};
    return it == m.end() ? nullptr : &it->second;
}//在"文件名->blob"这一张表里找key这个文件名，返回它的sha的指针

bool same(const std::string* a, const std::string* b){
    if(a == nullptr && b == nullptr) return true;
    if(a == nullptr || b == nullptr) return false;
    return *a == *b;
}//比较两个指针指向的sha是否相同

std::string conflictText(const std::string& a0, const std::string& b0){
    std::string a{a0};
    std::string b {b0};

    if(!a.empty() && a.back() != '\n') a += '\n';
    if(!b.empty() && b.back() != '\n') b += '\n';

    return "<<<<<<< HEAD\n" + a + "=======\n" + b + ">>>>>>>\n";
}//把冲突的两个文件拼接起来

}
Merge::Merge(Refs& refs, ObjectStore& store, Index& idx, const WorkTree& wt) : refs_{refs}, store_{store}, idx_{idx}, wt_{wt}, rules_{".gitliteignore"}{};

std::string Merge::findBase(const std::string& curSha, const std::string& givSha){
    //1.收集当前分支的全部祖先(DFS)
    std::set<std::string> ancestors;
    std::vector<std::string> stack{curSha};
    while(!stack.empty()){
        std::string id{stack.back()};
        stack.pop_back();
        if(!ancestors.insert(id).second) continue;
        Commit c{Commit::deserialize(store_.readObject(id))};
        for(const std::string& p : c.parents) stack.push_back(p);
    }
    //2.从givSha BFS,找第一个交集
    std::queue<std::string> q;
    q.push(givSha);
    std::set<std::string> seen;
    while(!q.empty()){
        std::string id{q.front()};
        q.pop();
        if(!seen.insert(id).second) continue;
        if(ancestors.count(id) > 0) return id;
        Commit c{Commit::deserialize(store_.readObject(id))};
        for(const std::string& p : c.parents){
            q.push(p);
        }
    }
    return curSha;
}//找两个分支的最近起点

void Merge::run(const std::string& given){
    //1.三个失败
    if(!idx_.empty()){
        Utils::message("You have uncommitted changes.");
        return;
    }
    if(!refs_.branchExists(given)){
        Utils::message("A branch with that name does not exist.");
        return;
    }
    const std::string curBranch{refs_.currentBranch()};
    if(given == curBranch){
        Utils::message("Cannot merge a branch with itself.");
        return;
    }
    //2.算base
    const std::string curSha{refs_.headCommit()};
    const std::string givSha{refs_.getBranch(given)};
    Commit cur{Commit::deserialize(store_.readObject(curSha))};
    Commit giv{Commit::deserialize(store_.readObject(givSha))};

    const std::string baseSha{findBase(curSha, givSha)};

    //3.若given是祖先
    if(baseSha == givSha){
        Utils::message("Given branch is an ancestor of the current branch.");
        return;
    }

    //4.若curBranch是given的祖先，快进
    if(baseSha == curSha){
        Checkout co{refs_, store_, idx_, wt_};
        if(!co.toSnapshot(giv)) return;
        refs_.setBranch(curBranch, givSha);
        idx_.clear();
        idx_.save();
        Utils::message("Current branch fast-forwarded.");
        return;
    }

    //5.普通合并
    //收集所有路径
    Commit base{Commit::deserialize(store_.readObject(baseSha))};

    std::set<std::string> paths;
    for(const auto& kv : base.snapshot) paths.insert(kv.first);
    for(const auto& kv : cur.snapshot) paths.insert(kv.first);
    for(const auto& kv : giv.snapshot) paths.insert(kv.first);

    //判定每一条路径保留、删、改、冲突
    std::map<std::string, std::string> next{cur.snapshot};//新的快照的底稿
    std::vector<std::pair<std::string, std::string>> writes;//要写进工作区的  文件名->内容
    std::set<std::string> deletes;//工作区删的 文件名
    bool conflict{false};

    for(const std::string& p :paths){
        const std::string* b{lookup(base.snapshot, p)};
        const std::string* c{lookup(cur.snapshot, p)};
        const std::string* g{lookup(giv.snapshot, p)};

        const bool curChanged(!same(c, b));
        const bool givChanged(!same(g, b));

        if(!givChanged) continue;
        if(!curChanged){
            if(g != nullptr){
                writes.push_back({p, store_.readObject(*g)});
                next[p] = *g;
            }
            else{
                deletes.insert(p);
                next.erase(p);
            }
            continue;
        }
        if(same(c, g)) continue;

        //冲突
        conflict = true;
        const std::string a{(c != nullptr)  ? store_.readObject(*c) : ""};
        const std::string b2 {(g != nullptr) ? store_.readObject(*g) : ""};
        const std::string merged{conflictText(a, b2)};
        const std::string blob{store_.writeBlob(merged)};
        writes.push_back({p, merged});
        next[p] = blob;
    }
    //未跟踪检查
    for(const auto& w : writes){
        const std::string& p{w.first};
        if(!wt_.exists(p)) continue;
        if(cur.snapshot.count(p) > 0) continue;
        if(rules_.isIgnored(p)) continue;
        Utils::message("There is an untracked file in the way; delete it, or add and commit it first.");
        return; 
    }
    //删
    for(const std::string& p : deletes){
        if(wt_.exists(p)) std::remove(p.c_str());
    }
    //写
    for(const auto& w : writes){
        Utils::writeContents(w.first, w.second);
    }
    //合并提交
    Commit nextCommit;
    nextCommit.parents = {curSha, givSha};
    nextCommit.timestamp = static_cast<long long>(std::time(nullptr));
    nextCommit.message = "Merged " + given + " into " + curBranch + ".";
    nextCommit.snapshot = next;

    const std::string sha{store_.writeCommit(nextCommit.serialize())};
    refs_.setBranch(curBranch, sha);
    idx_.clear();
    idx_.save();

    if(conflict){
        Utils::message("Encountered a merge conflict.");
    }
}//把given合进当前分支