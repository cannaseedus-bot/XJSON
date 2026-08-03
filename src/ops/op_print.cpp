#include "op_print.hpp"
#include <iostream>

using json = nlohmann::json;
using scope_t = std::unordered_map<std::string, json>;

void op_print(const json& step, scope_t& state) {
    // Support both "@in": "key" and "@in": ["key"]
    const json& in = step["@in"];
    std::string key;

    if (in.is_string()) {
        key = in.get<std::string>();
    } else if (in.is_array() && !in.empty()) {
        key = in[0].get<std::string>();
    }

    auto it = state.find(key);
    if (it != state.end()) {
        std::cout << key << " = " << it->second << "\n";
    } else {
        std::cout << key << " = <undefined>\n";
    }
}
