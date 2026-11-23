#ifndef WORKTREE_H
#define WORKTREE_H
#include<string>
#include<vector>

class WorkTree{
public:
    explicit WorkTree(const std::string& dir);

    std::vector<std::string> listFiles() const;
    bool exists(const std::string& f) const;
    std::string sha1Of(const std::string& f) const;
private:
    std::string dir_;
};
#endif