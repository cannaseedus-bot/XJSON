#pragma once
// sw.hpp -- Sidecar Store (SW)
//
// Loads sidecars.manifest.json and dispatches EXTERNAL_EXE sidecars: candidate/
// compute-only worker binaries (e.g. the Quantum stack) that read a JSON request
// on stdin and emit a JSON reply on stdout. In-process 'xcfe_manifest' sidecars
// are left to the existing SidecarLoader.
//
// Authority: the store never mutates its registry, never collapses/judges/promotes.
// It resolves a binary, spawns it, and returns whatever JSON it emits.
//
// Usage (CLI, built as the `sw` target):
//   sw --manifest sidecars.manifest.json list
//   sw --manifest sidecars.manifest.json describe quantum_hybrid
//   sw --manifest sidecars.manifest.json call quantum_hybrid extract_relations "{\"code\":\"...\"}"

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class SidecarStore {
public:
    // Load @sidecars from the store manifest. Returns false on failure.
    bool load(const std::string& manifest_path);

    // [{name, kind, ops, lane, phase, authority, available}]
    json list() const;

    // The registry entry for `name` + {resolved_bin, available}; error if unknown.
    json describe(const std::string& name) const;

    // Dispatch: external_exe -> spawn bin with {operation:op, ...body} on stdin,
    // parse the JSON reply. xcfe_manifest -> {status:"delegated_to_xcfe"}.
    // Missing/unknown -> {status:"unavailable"|"unknown_sidecar"} (never throws).
    json call(const std::string& name, const std::string& op, const json& body) const;

    // True if `name` is a registered external_exe sidecar (dispatched by this store,
    // not by the in-process SidecarLoader).
    bool is_external(const std::string& name) const {
        auto it = store_.find(name);
        return it != store_.end() && it->is_object() && it->value("kind", "") == "external_exe";
    }

private:
    json        store_;   // the @sidecars object
    std::string root_;    // directory of the manifest, for relative bin resolution

    std::string resolve_bin(const json& entry) const;
    json        call_external(const std::string& bin, const std::string& op, const json& body) const;
};
