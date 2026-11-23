#ifndef INDEX_H
#define INDEX_H

#include<map>
#include<set>
#include<string>

class Index{
public:
    explicit Index(const std::string& path);

    void load();
    void save() const;

    bool hasAdd(const std::string& f) const;
    bool hasRemove(const std::string& f) const;
    std::string getAddBlob(const std::string& f) const;

    void setAdd(const std::string& f, const std::string& blob);
    void setRemove(const std::string& f);
    void erase(const std::string& f);
    void clear();

    bool empty() const;
    const std::map<std::string, std::string>& adds() const {return adds_;}
    const std::set<std::string>& removes() const {return removes_;}
private:
    std::string path_;
    std::map<std::string, std::string> adds_;//文件名->blob sha
    std::set<std::string> removes_;//文件名
};

#endif