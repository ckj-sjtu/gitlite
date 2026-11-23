#ifndef OBJECT_STORE_H
#define OBJECT_STORE_H

#include<string>
#include<vector>
class ObjectStore{
public:
    explicit ObjectStore(const std::string& dir);
    bool exists(const std::string& sha) const;
    std::string writeBlob(const std::string& content);
    std::string writeCommit(const std::string& serialized);
    std::string readObject(const std::string& sha) const;
    std::vector<std::string> listObjects() const;
private:
    std::string dir_;
};

#endif