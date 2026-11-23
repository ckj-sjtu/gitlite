#ifndef REVISION_H
#define REVISION_H
#include<string>
class Refs;
class ObjectStore;
namespace Revision{
    std::string resolve(Refs& refs, ObjectStore& store, const std::string& spec);
}

#endif