#pragma once

#include <nlohmann/json.hpp>

nlohmann::json compile_gpu_kernel(const nlohmann::json& node);
