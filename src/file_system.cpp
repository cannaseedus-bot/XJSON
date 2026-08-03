#include "file_system.hpp"
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

json FileSystem::load_json(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("FileSystem: cannot open: " + path);
    }
    json j;
    try {
        f >> j;
    } catch (const json::exception& e) {
        throw std::runtime_error("FileSystem: JSON parse error in " + path + ": " + e.what());
    }
    return j;
}
