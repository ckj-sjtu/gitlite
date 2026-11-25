#ifndef REMOTE_H
#define REMOTE_H

#include<string>
class Remote{
public:
    explicit Remote(const std::string& gitliteDir);//".gitlite"

    bool exists(const std::string& name) const;
    std::string path(const std::string& name) const;

    void add(const std::string& name, const std::string& dir);
    void remove(const std::string& name);

    bool push(const std::string& name, const std::string& branch);
    bool fetch(const std::string& name, const std::string& branch);

private:
    bool resolve(const std::string& name, std::string& dir) const;

    std::string dir_;
};
#endif