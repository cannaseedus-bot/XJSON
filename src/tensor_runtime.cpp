#include "tensor_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

using json = nlohmann::json;

namespace {

constexpr std::size_t MAX_ELEMENTS = 16u * 1024u * 1024u;

std::mutex tensor_registry_mutex;
std::unordered_map<std::string, json> tensor_registry;
std::size_t tensor_registry_sequence = 0;

#ifdef _WIN32
using dml_gemm_bt_f32_fn = int (*)(const float*, const float*, float*, unsigned, unsigned, unsigned);

dml_gemm_bt_f32_fn load_dml_gemm() {
    static dml_gemm_bt_f32_fn function = nullptr;
    static bool attempted = false;
    if (attempted) return function;
    attempted = true;

    const char* override_path = std::getenv("KHANARY_DML_GEMM");
    HMODULE directml = LoadLibraryA("..\\ggml\\DirectML.dll");
    if (!directml) directml = LoadLibraryA("DirectML.dll");
    const char* path = override_path ? override_path : "..\\ggml\\dml_gemm.dll";
    HMODULE gemm = LoadLibraryA(path);
    if (gemm) {
        function = reinterpret_cast<dml_gemm_bt_f32_fn>(
            GetProcAddress(gemm, "dml_gemm_bt_f32"));
    }
    return function;
}
#endif

std::vector<std::size_t> shape_of(const json& tensor) {
    if (!tensor.is_object() || !tensor.contains("shape") || !tensor["shape"].is_array())
        throw std::runtime_error("TENSOR: expected an XJSON tensor with an array shape");
    std::vector<std::size_t> shape;
    for (const auto& dim : tensor["shape"]) {
        if (!dim.is_number_integer() || dim.get<long long>() <= 0)
            throw std::runtime_error("TENSOR: shape dimensions must be positive integers");
        shape.push_back(dim.get<std::size_t>());
    }
    if (shape.empty()) throw std::runtime_error("TENSOR: shape must not be empty");
    return shape;
}

std::size_t element_count(const std::vector<std::size_t>& shape) {
    std::size_t count = 1;
    for (const auto dim : shape) {
        if (dim > MAX_ELEMENTS / count)
            throw std::runtime_error("TENSOR: allocation exceeds element limit");
        count *= dim;
    }
    if (count > MAX_ELEMENTS)
        throw std::runtime_error("TENSOR: allocation exceeds element limit");
    return count;
}

std::vector<float> values_of(const json& tensor, std::size_t expected) {
    if (!tensor.contains("data") || !tensor["data"].is_array())
        throw std::runtime_error("TENSOR: CPU tensor data must be an array");
    if (tensor["data"].size() != expected)
        throw std::runtime_error("TENSOR: data length does not match shape");
    std::vector<float> values;
    values.reserve(expected);
    for (const auto& value : tensor["data"]) {
        if (!value.is_number())
            throw std::runtime_error("TENSOR: data must contain numeric values");
        values.push_back(value.get<float>());
    }
    return values;
}

json make_tensor(const std::vector<std::size_t>& shape, const std::vector<float>& values,
                 const std::string& phase = "Pop") {
    json shape_json = json::array();
    for (const auto dim : shape) shape_json.push_back(dim);
    json data = json::array();
    for (const auto value : values) data.push_back(value);
    return json{
        {"@type", "xjson/tensor"},
        {"shape", shape_json},
        {"dtype", "f32"},
        {"device", "cpu"},
        {"layout", "row_major"},
        {"phase", phase},
        {"data", data}
    };
}

json alloc_tensor(const json& node) {
    const json shape_value = node.contains("@shape") ? node["@shape"] : node.value("shape", json::array());
    if (!shape_value.is_array()) throw std::runtime_error("TENSOR: expected @shape to be an array");
    json shape_tensor = json::object();
    shape_tensor["shape"] = shape_value;
    const auto shape = shape_of(shape_tensor);
    const auto count = element_count(shape);
    const float fill = node.value("@fill", node.value("fill", 0.0f));
    return make_tensor(shape, std::vector<float>(count, fill), node.value("@phase", "Pop"));
}

json matmul(const json& node) {
    const auto& a = node.contains("@a") ? node["@a"] : node.at("a");
    const auto& b = node.contains("@b") ? node["@b"] : node.at("b");
    const auto ashape = shape_of(a);
    const auto bshape = shape_of(b);
    if (ashape.size() != 2 || bshape.size() != 2 || ashape[1] != bshape[0])
        throw std::runtime_error("MATMUL: expected compatible 2D tensors");
    const auto av = values_of(a, element_count(ashape));
    const auto bv = values_of(b, element_count(bshape));
    std::vector<float> out(ashape[0] * bshape[1], 0.0f);
    std::string backend = "cpu-fallback";

#ifdef _WIN32
    if (auto dml = load_dml_gemm()) {
        // Khanary's ABI expects B row-major as [N,K], while XJSON uses [K,N].
        std::vector<float> b_transposed(bv.size());
        for (std::size_t k = 0; k < bshape[0]; ++k)
            for (std::size_t col = 0; col < bshape[1]; ++col)
                b_transposed[col * bshape[0] + k] = bv[k * bshape[1] + col];
        if (dml(av.data(), b_transposed.data(), out.data(),
                static_cast<unsigned>(ashape[0]),
                static_cast<unsigned>(bshape[1]),
                static_cast<unsigned>(ashape[1])) == 0) {
            backend = "khanary-directml";
        }
    }
#endif

    if (backend == "cpu-fallback") {
        for (std::size_t row = 0; row < ashape[0]; ++row)
            for (std::size_t col = 0; col < bshape[1]; ++col)
                for (std::size_t k = 0; k < ashape[1]; ++k)
                    out[row * bshape[1] + col] += av[row * ashape[1] + k] * bv[k * bshape[1] + col];
    }
    json result = make_tensor({ashape[0], bshape[1]}, out, "Sek");
    result["op"] = "gemm";
    result["backend"] = backend;
    return result;
}

json unary(const json& node, const std::string& op) {
    const auto& input = node.contains("@in") ? node["@in"] : node.at("input");
    const auto shape = shape_of(input);
    auto values = values_of(input, element_count(shape));
    if (op == "relu") {
        for (auto& value : values) value = std::max(0.0f, value);
    } else if (op == "softmax") {
        if (shape.empty()) throw std::runtime_error("SOFTMAX: tensor must have a dimension");
        const auto width = shape.back();
        for (std::size_t offset = 0; offset < values.size(); offset += width) {
            const auto max_value = *std::max_element(values.begin() + offset, values.begin() + offset + width);
            float sum = 0.0f;
            for (std::size_t i = 0; i < width; ++i) {
                values[offset + i] = std::exp(values[offset + i] - max_value);
                sum += values[offset + i];
            }
            for (std::size_t i = 0; i < width; ++i) values[offset + i] /= sum;
        }
    } else {
        throw std::runtime_error("TENSOR: unknown unary operation: " + op);
    }
    return make_tensor(shape, values, "Sek");
}

json registry_operation(const json& node) {
    const std::string raw_fn = node.value("@fn", "tensor_list");
    const std::string fn = raw_fn == "tensor_register" ? "register" :
                           raw_fn == "tensor_get" ? "get" :
                           raw_fn == "tensor_exists" ? "exists" :
                           raw_fn == "tensor_remove" ? "remove" : "list";
    const std::string id = node.value("@id", node.value("id", node.value("@tensor", "")));
    std::lock_guard<std::mutex> lock(tensor_registry_mutex);

    if (fn == "register") {
        const json& value = node.contains("@value") ? node["@value"] : node.at("value");
        if (!value.is_object() || value.value("@type", "") != "xjson/tensor")
            throw std::runtime_error("TENSOR_REGISTER: expected an XJSON tensor value");
        std::string tensor_id = id;
        if (tensor_id.empty()) {
            tensor_id = value.value("identity", "");
        }
        if (tensor_id.empty()) {
            tensor_id = "tensor://" + std::to_string(++tensor_registry_sequence);
        }
        json stored = value;
        stored["identity"] = tensor_id;
        tensor_registry[tensor_id] = stored;
        return stored;
    }
    if (fn == "get") {
        auto it = tensor_registry.find(id);
        if (it == tensor_registry.end()) throw std::runtime_error("TENSOR_GET: tensor not found: " + id);
        return it->second;
    }
    if (fn == "exists") return tensor_registry.find(id) != tensor_registry.end();
    if (fn == "remove") {
        const auto removed = tensor_registry.erase(id);
        if (removed == 0) throw std::runtime_error("TENSOR_REMOVE: tensor not found: " + id);
        return true;
    }
    if (fn == "list") {
        json ids = json::array();
        for (const auto& [tensor_id, _] : tensor_registry) ids.push_back(tensor_id);
        return ids;
    }
    throw std::runtime_error("TENSOR: unsupported registry function: " + fn);
}

} // namespace

json tensor_runtime(const json& node) {
    const std::string fn = node.value("@fn", "alloc");
    if (fn == "alloc") return alloc_tensor(node);
    if (fn == "matmul") return matmul(node);
    if (fn == "relu" || fn == "softmax") return unary(node, fn);
    if (fn == "tensor_register" || fn == "tensor_get" || fn == "tensor_exists" ||
        fn == "tensor_remove" || fn == "tensor_list") return registry_operation(node);
    if (fn == "copy") {
        const auto& source = node.contains("@tensor") ? node["@tensor"] : node.at("tensor");
        const auto device = node.value("@device", node.value("device", "cpu"));
        if (device != "cpu") throw std::runtime_error("TENSOR_COPY: only cpu residency is implemented");
        return source;
    }
    throw std::runtime_error("TENSOR: unsupported function: " + fn);
}
