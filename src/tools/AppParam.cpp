#include "AppParam.h"


std::unordered_map<std::string, ParamMeta> AppParam::params;
std::unordered_map<std::string, std::string> AppParam::aliasMap;

void AppParam::registerParam(std::string key, std::vector<std::string> aliases, std::string description) {
    ParamMeta meta;
    meta.key = key;
    meta.aliases = aliases;
    meta.description = description;

    params[key] = meta;

    for (const auto& alias : aliases) {
        aliasMap[alias] = key;
    }

    // Also register the full key name with -- prefix
    if(key.length() > 4) aliasMap["--" + key] = key;
    else aliasMap["-" + key] = key;
}


void AppParam::initialize(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Handle --key=value or -k=value
        size_t equalPos = arg.find('=');
        std::string rawKey = (equalPos != std::string::npos) ? arg.substr(0, equalPos) : arg;

        // Handle aliases
        auto keyIt = aliasMap.find(rawKey);
        if (keyIt != aliasMap.end()) {
            std::string key = keyIt->second;
            std::string value;

            if (equalPos != std::string::npos) {
                value = arg.substr(equalPos + 1);
            } else if ((i + 1) < argc && argv[i + 1][0] != '-') {
                value = argv[++i];
            } else {
                value = "true"; // treat it as a flag
            }

            // Remove quotes if needed
            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }

            params[key].value = value;
            params[key].isSet = true;
        } else {
            // Handle positional args or unknown params
            params[""].value = arg;
        }
    }
}

std::string AppParam::get(const std::string& key) {
    auto it = params.find(key);
    if (it != params.end()) {
        return it->second.value;
    }
    return "";
}

 const std::unordered_map<std::string, ParamMeta>& AppParam::getAll() {
    return params;
}

bool AppParam::has(const std::string& key) {
    auto it = params.find(key);
    return it != params.end() && it->second.isSet;
}


std::vector<std::string> AppParam::getArray(const std::string& key) {
    auto it = params.find(key);
    if (it == params.end()) return {};

    std::string raw = it->second.value;

    // Remove outer brackets
    if (raw.front() == '[' && raw.back() == ']')
        raw = raw.substr(1, raw.size() - 2);

    std::vector<std::string> result;

    std::regex re(R"((\".*?\"|\S+))"); // Match quoted strings or non-whitespace tokens
    auto begin = std::sregex_iterator(raw.begin(), raw.end(), re);
    auto end = std::sregex_iterator();

    for (auto i = begin; i != end; ++i) {
        std::string token = (*i)[1].str();

        // Remove quotes if present
        if (!token.empty() && token.front() == '"' && token.back() == '"') {
            token = token.substr(1, token.size() - 2);
        }

        result.push_back(token);
    }

    return result;
}

