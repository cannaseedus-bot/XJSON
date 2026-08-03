#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>

// Legacy direct-dispatch PRINT
void op_print(const nlohmann::json& step,
              std::unordered_map<std::string, nlohmann::json>& state);
