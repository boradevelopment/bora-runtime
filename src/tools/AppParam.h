#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <regex>

struct ParamMeta {
    std::string key;                 // e.g. "out"
    std::vector<std::string> aliases; // e.g. {"-o"}
    std::string value;
    std::string description;
    bool isSet = false;
};

class AppParam {
public:
    static void initialize(int argc, char** argv);
    static void registerParam(std::string key, std::vector<std::string> aliases = {}, std::string description = "");
    static std::string get(const std::string& key);
    static const std::unordered_map<std::string, ParamMeta>& getAll();
    template<typename T>
    static T getValue(const std::string& key){
        auto it = params.find(key);
        if (it == params.end()) return T();


        std::istringstream ss(it->second.value);
        T result;
        ss >> std::boolalpha >> result;
        if (ss.fail()) return T();
        return result;
    }

    static bool has(const std::string& key);
    // Parses --array=["string", 1, true]
    static std::vector<std::string> getArray(const std::string& key);


private:
    static std::unordered_map<std::string, ParamMeta> params; // key → ParamMeta
    static std::unordered_map<std::string, std::string> aliasMap; // alias → key
};

