#ifndef CHECKOUT_H
#define CHECKOUT_H

#include<string>
#include "IgnoreRules.h"

class Refs;
class ObjectStore;
class Index;
class WorkTree;
class Commit;

class Checkout{
public:
    Checkout(Refs& refs, ObjectStore& store, Index& idx, const WorkTree& wt);

    bool toSnapshot(const Commit& target);

    bool fileTo(const Commit& commit, const std::string& f);

private:
    bool untrackedInTheWay(const Commit& cur, const Commit& target) const;

    Refs& refs_;
    ObjectStore& store_;
    Index& idx_;
    const WorkTree& wt_;
    IgnoreRules rules_;
};

#endif