#ifndef IGNORE_RULES_H
#define IGNORE_RULES_H

#include<string>
#include<vector>

class IgnoreRules{
public:
    explicit IgnoreRules(const std::string& path);
    bool isIgnored(const std::string& filename) const;
private:
    struct Rule{
        bool negated;
        std::string pattern;
    };
    std::vector<Rule> rules_;

    static bool matchRec(const std::string& p, size_t pi, const std::string& s, size_t si);
    static bool match(const std::string& pattern, const std::string& filename);
};
#endif