#ifndef STATUS_H
#define STATUS_H

#include<string>
#include<vector>

class Refs;
class ObjectStore;
class Index;
class WorkTree;

class Status{
public:
    Status(Refs& refs, ObjectStore& store, Index& idx, const WorkTree& wt);
    void print() const;

private:
    std::string current_;
    std::vector<std::string> branches_;
    std::vector<std::string> staged_;
    std::vector<std::string> removed_;
    std::vector<std::string> modified_;
    std::vector<std::string> untracked_;
};

#endif