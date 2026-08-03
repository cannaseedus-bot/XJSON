#include "gpu_dispatch.hpp"

#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <d3dcompiler.h>
#endif

using json = nlohmann::json;

json compile_gpu_kernel(const json& node) {
    const json& source_value = node.contains("@source") ? node["@source"] : node.value("@kernel", json());
    if (!source_value.is_string()) {
        throw std::runtime_error("GPU_DISPATCH: expected @source or string @kernel");
    }

    const std::string source = source_value.get<std::string>();
    const std::string entry = node.value("@entry", "main");
    const std::string profile = node.value("@profile", "cs_5_0");

#ifdef _WIN32
    ID3DBlob* bytecode = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT result = D3DCompile(
        source.data(), source.size(), nullptr, nullptr, nullptr,
        entry.c_str(), profile.c_str(), 0, 0, &bytecode, &errors);

    if (FAILED(result)) {
        std::string message = "D3DCompile failed";
        if (errors != nullptr) {
            message += ": ";
            message.append(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize());
            errors->Release();
        }
        if (bytecode != nullptr) bytecode->Release();
        throw std::runtime_error(message);
    }

    const auto byte_count = bytecode->GetBufferSize();
    if (errors != nullptr) errors->Release();
    bytecode->Release();

    return json{
        {"compiled", true},
        {"compiler", "D3DCompiler_47.dll"},
        {"entry", entry},
        {"profile", profile},
        {"bytecode_bytes", byte_count},
        {"dispatch", "compile-only; device context required"}
    };
#else
    return json{
        {"compiled", false},
        {"available", false},
        {"reason", "D3DCompiler_47.dll requires Windows"}
    };
#endif
}
