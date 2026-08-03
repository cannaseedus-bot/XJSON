#pragma once
// sidecar.hpp -- JSON Sidecar ABI
//
// Sidecar = a specialized SCO with INIT / CALL / TICK entrypoints.
// It is also the bootstrap contract for JSON apps/games/programs/websites:
//   json_runtime.exe --manifest manifest.json --sidecar <app>.manifest.json
//
// Any app manifest sidecar may declare @routes/@ports/@schema/@entry/@assets.
// Micronaut sidecars may additionally declare @micronaut/@beans/@schedules/@events.
// The runtime treats that JSON as executable notation once it is backed by @ops,
// dispatch schemas, or route bindings.
//
// JSON sidecar file format:
//   {
//     "@sidecar": {
//       "id":           "gpu.dx12",
//       "capabilities": ["GPU","TENSOR"],
//       "version":      "1.0.0"
//     },
//     "@ops": {
//       "MATMUL": { "@type":"composed", "@in":["A","B"], "@exec":{ ... } }
//     },
//     "@init": [
//       { "@op":"LOG", "@args":["gpu.dx12 sidecar ready"] }
//     ]
//   }
//
// Usage:
//   SidecarLoader loader;
//   loader.load("sco/sidecars/gpu.json", "gpu");
//   json result = loader.call("gpu", "MATMUL", scope);

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>
#include <memory>

using json    = nlohmann::json;
using scope_t = std::unordered_map<std::string, json>;

class XCFE;  // forward

struct Sidecar {
    std::string         name;
    json                capability;         // @sidecar block
    json                op_defs;            // @ops block
    std::unique_ptr<XCFE> xcfe;            // inner executor
    bool                initialized = false;
};

class SidecarLoader {
public:
    SidecarLoader();
    ~SidecarLoader();

    // Load sidecar from a JSON file; run @init; register ops.
    // Returns false on failure.
    bool load(const std::string& path, const std::string& name,
              const json* stdlib_ops = nullptr);

    // Dispatch: call op_name on named sidecar
    json call(const std::string& sidecar_name,
              const std::string& op_name,
              scope_t& scope);

    // Check if a sidecar is loaded
    bool has(const std::string& name) const;

    // Get capability descriptor
    json capability(const std::string& name) const;

    // List loaded sidecars
    std::vector<std::string> list() const;

    // Access raw sidecar (for op merging into parent XCFE)
    const Sidecar* get(const std::string& name) const;

private:
    std::unordered_map<std::string, Sidecar> sidecars_;
};
