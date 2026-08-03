#pragma once
#include <nlohmann/json.hpp>
#include <string>

class FileSystem {
public:
    static nlohmann::json load_json(const std::string& path);
};
