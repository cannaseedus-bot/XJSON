#pragma once

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class BotRuntime {
public:
    static bool is_bot_manifest(const json& descriptor);
    static json contract();
    static json build_payload(const json& node, const json& input);
};
