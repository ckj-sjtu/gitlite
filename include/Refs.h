#ifndef REFS_H
#define REFS_H

#include<string>
#include<vector>

class Refs{
public:
    explicit Refs(const std::string& gitliteDir);

    std::string currentBranch() const;
    std::string headCommit() const;
    void setHeadBranch(const std::string& name);

    bool branchExists(const std::string& name) const;
    std::string getBranch(const std::string& name) const;
    void setBranch(const std::string& name, const std::string& sha);

    std::vector<std::string> listBranches() const;
    std::vector<std::string> listTags() const;

    bool tagExists(const std::string& name) const;
    std::string getTag(const std::string& name) const;
    void setTag(const std::string& name, const std::string& sha);
    
    void deleteBranch(const std::string& name);
private:
    std::string dir_;
};
#endif