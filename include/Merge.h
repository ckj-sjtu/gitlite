#ifndef MERGE_H
#define MERGE_H

#include<string>
#include<vector>

#include "IgnoreRules.h"
class Refs;
class ObjectStore;
class Index;
class WorkTree;
class Commit;

class Merge{
public:
    Merge(Refs& refs, ObjectStore& store, Index& idx, const WorkTree& wt);

    void run(const std::string& given);

private:
    std::string findBase(const std::string& curSha, const std::string& givSha);

    Refs& refs_;
    ObjectStore& store_;
    Index& idx_;
    const WorkTree& wt_;
    IgnoreRules rules_;
};

#endif