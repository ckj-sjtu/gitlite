#include"../include/IgnoreRules.h"
#include"../include/Utils.h"
#include<sstream>

IgnoreRules::IgnoreRules(const std::string& path){
    if(!Utils::exists(path)) return;
    std::istringstream in(Utils::readContentsAsString(path));
    std::string line;
    while(std::getline(in, line)){
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(line.empty()) continue;
        Rule r{false, ""};
        size_t i{0};
        if(line[0] == '!'){
            r.negated = true;
            i = 1;
        }
        else if(line[0] == '#') continue;
        std::string pat{line.substr(i)};
        if(!pat.empty() && pat[0] == '/') pat = pat.substr(1);
        r.pattern = pat;
        rules_.push_back(r);
    }
}
bool IgnoreRules::isIgnored(const std::string& filename) const{
    bool ignore{false};
    for(const auto& r : rules_){
        if(match(r.pattern, filename)){
            ignore = !r.negated;
        }
    }
    return ignore;
}
bool IgnoreRules::matchRec(const std::string& p, size_t pi, const std::string& s, size_t si){
    while(pi < p.size()){
        if(p[pi] == '\\' && pi + 1 < p.size()){
            if(si >= s.size() || p[pi + 1] != s[si]) return false;
            pi += 2;
            si++;
        }
        else if(p[pi] == '?'){
            if(si >= s.size()) return false;
            ++pi;
            ++si;
        }
        else if(p[pi] == '*'){
            for(size_t k = si; k <= s.size(); ++k){
                if(matchRec(p, pi + 1, s, k)) return true;
            }
            return false;
        }
        else{
            if(si >= s.size() || p[pi] != s[si]) return false;
            ++pi;
            ++si;
        }
    }
    return si == s.size();
}
bool IgnoreRules::match(const std::string& pattern, const std::string& filename){
    return matchRec(pattern, 0, filename, 0);
}