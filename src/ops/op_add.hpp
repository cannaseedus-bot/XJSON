#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>

// Legacy direct-dispatch ADD — used when op is not in @ops registry.
void op_add(const nlohmann::json& step,
            std::unordered_map<std::string, nlohmann::json>& state);
