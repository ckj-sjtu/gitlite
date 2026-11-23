#ifndef COMMIT_H
#define COMMIT_H
#include<map>
#include<string>
#include<vector>
// 建议类：抽象一次提交（消息、时间戳、父提交、文件快照），可自由设计或删除。
// tag 是指向提交的仓库引用，不属于 Commit 对象自身。
class Commit{
public:
    std::vector<std::string> parents;
    long long timestamp{0};
    std::string message;
    std::map<std::string, std::string> snapshot;//文件名->sha

    std::string serialize() const;
    static Commit deserialize(const std::string& data);

    std::string logEntry(const std::string& id) const;
};
#endif // COMMIT_H
