#pragma once

#include <nlohmann/json.hpp>
#include <string>

class RuntimeAuthority {
public:
    static nlohmann::json phase_contract();
    static nlohmann::json sidecar_examples();
    static nlohmann::json app_contract(const nlohmann::json& manifest);
    static std::string classify_sidecar(const nlohmann::json& capability);
};
