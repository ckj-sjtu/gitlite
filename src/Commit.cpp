#include "../include/Commit.h"
#include<sstream>
#include<ctime>

// 建议类 Commit 的实现文件，可按需使用或删除
namespace{
std::string formatDate(long long ts){
    std::time_t t{static_cast<std::time_t>(ts)};
    std::tm* tm{std::localtime(&t)};
    if(tm == nullptr) return "Thu Jan 01 00:00:00 1970 +0000";
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y %z", tm);
    return buf;
}//把Unix秒转成本地时区的英文时间串
}
std::string Commit::serialize() const{
    std::string s{"commit\n"};
    for(const auto& p : parents){
        s += "parent " + p + "\n";
    }
    s += "timestamp " + std::to_string(timestamp) + "\n";
    s += "message " + message + "\n";
    s += "\n";
    for(const auto &[name, blob] : snapshot){
        s +=  name + " " + blob + "\n";
    }
    return s;
}

Commit Commit::deserialize(const std::string& data){
    Commit c;
    std::istringstream in(data);
    std::string line;
    std::getline(in, line);

    while(std::getline(in, line)){
        if(line.empty()) break;
        if(line.rfind("parent ", 0) == 0){
            c.parents.push_back(line.substr(7));
        }
        if(line.rfind("timestamp ", 0) == 0){
            c.timestamp = std::stoll(line.substr(10));
        }
        if(line.rfind("message ", 0) == 0){
            c.message = line.substr(8);
        }
    }
    while(std::getline(in, line)){
        if(line.empty()) continue;
        if(line.size() <= 41) continue;
        std::string blob = line.substr(line.size() - 40);
        std::string name = line.substr(0, line.size() - 41);
        c.snapshot[name] = blob;
    }
    return c;
}
std::string Commit::logEntry(const std::string& id) const{
    std::string s{"===\ncommit " + id + "\n"};
    if(parents.size() == 2){
        s += "Merge: " + parents[0].substr(0, 7) + " " + parents[1].substr(0, 7) + "\n";
    }
    s += "Date: " + formatDate(timestamp) + "\n";
    s += message + "\n";
    s += "\n";
    return s;
}//把commit变成日志
