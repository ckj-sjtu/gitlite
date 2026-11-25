#include<iostream>
#include<algorithm>
#include "../include/SomeObj.h"
#include "../include/Commit.h"
#include "../include/ObjectStore.h"
#include "../include/Index.h"
#include "../include/Refs.h"
#include "../include/IgnoreRules.h"
#include "../include/Utils.h"
#include "../include/Revision.h"
#include "../include/WorkTree.h"
#include "../include/Status.h"
#include "../include/Checkout.h"
#include "../include/Merge.h"
#include "../include/Remote.h"
#include<ctime>
SomeObj::SomeObj() {}

// ---------------- Subtask 1：init / add / commit / rm ----------------

void SomeObj::init() {
    // TODO: 创建 .gitlite 目录、initial commit 与 master 分支
    if(Utils::exists(".gitlite")){
        Utils::message("A Gitlite version-control system already exists in the current directory.");
        return;
    }
    Utils::createDirectories(".gitlite/objects");
    Utils::createDirectories(".gitlite/refs/branches");
    Utils::createDirectories(".gitlite/refs/tags");
    Utils::writeContents(".gitlite/HEAD", "ref: refs/branches/master\n");
    Utils::writeContents(".gitlite/index", "");

    Commit initial;
    initial.parents = {};
    initial.timestamp = 0;
    initial.snapshot = {};
    initial.message  ="initial commit";

    ObjectStore store{".gitlite/objects"};
    std::string sha = store.writeCommit(initial.serialize());
    Utils::writeContents(".gitlite/refs/branches/master", sha + "\n");
}

void SomeObj::add(const std::string& f) {
    // TODO: 将文件的当前内容加入暂存区
    if(!Utils::isFile(f)){
        Utils::message("File does not exist.");
        return;
    }
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();

    Commit head = Commit::deserialize(store.readObject(refs.headCommit()));
    bool tracked = head.snapshot.count(f) > 0;
    bool stagedAdd = idx.hasAdd(f);
    bool stagedRemove = idx.hasRemove(f);

    IgnoreRules rules{".gitliteignore"};
    if(rules.isIgnored(f) && !tracked && !stagedAdd && !stagedRemove){
        return;
    }

    std::string content{Utils::readContentsAsString(f)};
    std::string blob{store.writeBlob(content)};

    if(tracked && head.snapshot.at(f) == blob){
        idx.erase(f);
        idx.save();
        return;
    }
    idx.setAdd(f, blob);
    idx.save();
}

void SomeObj::commit(const std::string& m) {
    // TODO: 将暂存区保存为新提交，并推进当前分支
    if(m.empty()){
        Utils::message("Please enter a commit message.");
        return;
    }
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();

    if(idx.empty()){
        Utils::message("No changes added to the commit.");
        return;
    }

    std::string parentSha{refs.headCommit()};
    Commit parent{Commit::deserialize(store.readObject(parentSha))};

    Commit next;
    next.parents = {parentSha};
    next.timestamp = static_cast<long long>(std::time(nullptr));
    next.message = m;
    next.snapshot = parent.snapshot;

    for(const auto& [name, blob] : idx.adds()){
        next.snapshot[name] = blob;
    }
    for(const auto& name : idx.removes()){
        next.snapshot.erase(name);
    }

    std::string sha{store.writeCommit(next.serialize())};
    refs.setBranch(refs.currentBranch(), sha);
    idx.clear();
    idx.save();
}

void SomeObj::rm(const std::string& f) {
    // TODO: 将文件取消暂存，或标记为待删除
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();

    Commit head{Commit::deserialize(store.readObject(refs.headCommit()))};
    bool tracked{head.snapshot.count(f) > 0};
    bool stagedAdd{idx.hasAdd(f)};

    if(stagedAdd && !tracked){
        idx.erase(f);
        idx.save();
        return;
    }
    if(tracked){
        idx.erase(f);
        idx.setRemove(f);
        idx.save();
        if(Utils::exists(f)){
            Utils::restrictedDelete(f);
        }
        return;
    }
    
    Utils::message("No reason to remove the file.");
}

// ---------------- Subtask 2：log / global-log / tag ----------------

void SomeObj::log() {
    // TODO: 从当前提交沿第一个父提交打印日志
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};

    std::string id{refs.headCommit()};
    while(!id.empty()){
        Commit c{Commit::deserialize(store.readObject(id))};
        std::cout << c.logEntry(id);
        if(c.parents.empty()) break;
        id = c.parents[0];
    }
}

void SomeObj::globalLog() {
    // TODO: 从所有本地分支、远程跟踪分支和标签出发，输出全部可达提交并按规范排序
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};

    std::vector<std::string> stack;
    for(const std::string& b : refs.listBranches()) stack.push_back(refs.getBranch(b));
    for(const std::string& t : refs.listTags()) stack.push_back(refs.getTag(t)); 

    std::map<std::string, Commit> commits;
    while(!stack.empty()){
        std::string id{stack.back()};
        stack.pop_back();

        if(id.empty() || commits.count(id) > 0) continue;

        Commit c{Commit::deserialize(store.readObject(id))};
        for(const std::string& p : c.parents) stack.push_back(p);

        commits[id] = c;
    }

    std::vector<std::pair<long long, std::string>> order;
    for(const auto& kv : commits){
        order.push_back({-kv.second.timestamp, kv.first});
    }
    std::sort(order.begin(), order.end());

    for(const auto& it : order){
        std::cout << commits.at(it.second).logEntry(it.second);
    }
}

void SomeObj::listTags() {
    // TODO: 按字典序打印所有标签及其目标提交 id
    Refs refs{".gitlite"};
    for(const std::string& t : refs.listTags()){
        std::cout << t << " " << refs.getTag(t) << "\n";
    }
}

void SomeObj::tag(const std::string& n) {
    // TODO: 为当前提交创建轻量标签
    Refs refs{".gitlite"};
    if(refs.tagExists(n)){
        Utils::message("A tag with that name already exists.");
        return;
    }
    refs.setTag(n, refs.headCommit());
}

void SomeObj::tag(const std::string& n, const std::string& rev) {
    // TODO: 为指定提交或标签所指提交创建轻量标签
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};

    if(refs.tagExists(n)){
        Utils::message("A tag with that name already exists.");
        return;
    }
    std::string sha{Revision::resolve(refs, store, rev)};
    if(sha.empty()){
        Utils::message("No commit with that id exists.");
        return;
    }
    refs.setTag(n, sha);
}

// ---------------- Subtask 3：status / branch / rm-branch ----------------

void SomeObj::status() {
    // TODO: 显示分支、暂存区、未暂存修改与未跟踪文件
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();
    WorkTree wt{"."};
    Status status{refs, store, idx, wt};
    status.print();
}

void SomeObj::branch(const std::string& b) {
    // TODO: 新建指向当前提交的分支，不切换
    Refs refs{".gitlite"};
    Remote rem{".gitlite"};
    if(refs.branchExists(b) || rem.exists(b)){
        Utils::message("A branch with that name already exists.");
        return;
    }
    refs.setBranch(b, refs.headCommit());
}

void SomeObj::rmBranch(const std::string& b) {
    // TODO: 删除分支指针，不删除其提交
    Refs refs{".gitlite"};
    if(!refs.branchExists(b)){
        Utils::message("A branch with that name does not exist.");
        return;
    }
    if(refs.currentBranch() == b){
        Utils::message("Cannot remove the current branch.");
        return;
    }
    refs.deleteBranch(b);
}

// ---------------- Subtask 4：checkout / reset ----------------

void SomeObj::checkoutBranch(const std::string& b) {
    // TODO: 切换到指定分支
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();
    WorkTree wt{"."};

    if(!refs.branchExists(b)){
        Utils::message("No such branch exists.");
        return;
    }
    if(b == refs.currentBranch()){
        Utils::message("No need to checkout the current branch.");
        return;
    }

    Commit target{Commit::deserialize(store.readObject(refs.getBranch(b)))};
    Checkout co{refs, store, idx, wt};

    if(!co.toSnapshot(target)){
        return;
    }
    refs.setHeadBranch(b);
    idx.clear();
    idx.save();
}

void SomeObj::checkoutFile(const std::string& f) {
    // TODO: 用当前提交中的版本覆盖工作目录中的文件
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();
    WorkTree wt{"."};

    Commit head{Commit::deserialize(store.readObject(refs.headCommit()))};
    Checkout co{refs, store, idx, wt};
    if(!co.fileTo(head, f)){
        return;
    }
}

void SomeObj::checkoutFileInCommit(const std::string& rev, const std::string& f) {
    // TODO: 用指定 revision 的版本覆盖工作文件，支持标签与 id 缩写
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();
    WorkTree wt{"."};

    std::string sha{Revision::resolve(refs, store, rev)};
    if(sha.empty()){
        Utils::message("No commit with that id exists.");
        return;
    }
    Commit commit{Commit::deserialize(store.readObject(sha))};
    Checkout co{refs, store, idx, wt};
    if(!co.fileTo(commit, f)){
        return;
    }
}

void SomeObj::reset(const std::string& rev) {
    // TODO: 解析 revision，检出指定提交并移动当前分支头
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();
    WorkTree wt{"."};
    

    std::string sha{Revision::resolve(refs, store, rev)};
    if(sha.empty()){
        Utils::message("No commit with that id exists.");
        return;
    }
    Commit target{Commit::deserialize(store.readObject(sha))};
    
    Checkout co{refs, store, idx, wt};
    if(!co.toSnapshot(target)){
        return;
    }

    refs.setBranch(refs.currentBranch(), sha);
    idx.clear();
    idx.save();
}

// ---------------- Subtask 5：merge ----------------

void SomeObj::merge(const std::string& b) {
    // TODO: 将指定分支合并到当前分支
    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();
    WorkTree wt{"."};

    Merge merge{refs, store, idx, wt};
    merge.run(b);
}

// ---------------- Bonus-1：add-remote / rm-remote / push / fetch / pull ----------------

void SomeObj::addRemote(const std::string& n, const std::string& p) {
    // TODO: 保存远程仓库地址
    Refs refs{".gitlite"};
    Remote rem{".gitlite"};

    if(rem.exists(n) || refs.branchExists(n)){
        Utils::message("A remote with that name already exists.");
        return;
    }
    rem.add(n, p);
}

void SomeObj::rmRemote(const std::string& n) {
    // TODO: 删除远程仓库信息
    Remote rem{".gitlite"};
    if(!rem.exists(n)){
        Utils::message("A remote with that name does not exist.");
        return;
    }
    rem.remove(n);
}

void SomeObj::push(const std::string& n, const std::string& b) {
    // TODO: 将当前分支的提交推送到远程分支
    Remote rem{".gitlite"};
    rem.push(n, b);
}

void SomeObj::fetch(const std::string& n, const std::string& b) {
    // TODO: 将远程分支的提交与 blob 复制到本地
    Remote rem{".gitlite"};
    rem.fetch(n, b);
}

void SomeObj::pull(const std::string& n, const std::string& b) {
    // TODO: fetch 后将取回的分支合并到当前分支
    Remote rem{".gitlite"};
    if(!rem.fetch(n ,b)){
        return;
    }

    Refs refs{".gitlite"};
    ObjectStore store{".gitlite/objects"};
    Index idx{".gitlite/index"};
    idx.load();
    WorkTree wt{"."};

    Merge merge{refs, store, idx, wt};
    merge.run(n + "/" + b);
}

// ---------------- Bonus-2：diff / show ----------------

void SomeObj::diff() {
    // TODO: 比较当前提交与工作目录并输出差异，无差异时不输出
}

void SomeObj::diffWithCommit(const std::string&) {
    // TODO: 解析 revision，比较指定提交与工作目录，无差异时不输出
}

void SomeObj::diffBetween(const std::string&, const std::string&) {
    // TODO: 解析两个 revision 并比较对应提交，无差异时不输出
}

void SomeObj::show() {
    // TODO: 显示当前提交及其相对第一父提交的差异
}

void SomeObj::show(const std::string&) {
    // TODO: 显示指定提交或标签及其相对第一父提交的差异
}
