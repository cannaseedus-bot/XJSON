#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

using json    = nlohmann::json;
using scope_t = std::unordered_map<std::string, json>;
using OpFn    = std::function<json(const json&, scope_t&)>;

// Forward declarations
class SCORegistry;
class SidecarLoader;
struct Sidecar;

// ── XCFE — eXecutable Computation Framework Engine ────────────────────────────
//
// Five native primitives (C++):
//   native.READ   – scope/global read
//   native.WRITE  – scope/global write
//   native.EVAL   – semantic expander (math/logic/flow/io/meta/@expr)
//   native.HASH   – SHA-256 content hash
//   native.CALL   – external dispatch via URI (sco:// sidecar:// http://)
//
// All standard ops (ADD, IF, LOOP, …) are JSON-defined in stdlib.json.

class XCFE {
public:
    XCFE();

    // Execute a full @program object
    void execute(const json& program);

    // Recursive evaluator — the core of the runtime
    json eval(const json& node, scope_t& scope);

    // Op registry (JSON-defined ops)
    void load_ops(const json& ops_map);
    std::vector<std::string> list_ops() const;

    // Apply a delta stream to a program object
    json apply_delta(json program, const json& delta_ops);

    // Persistent state across execute() calls
    scope_t global_state;

    // Optional subsystem pointers (wired by Runtime)
    SCORegistry*   sco      = nullptr;
    SidecarLoader* sidecars = nullptr;

private:
    std::unordered_map<std::string, json>  op_defs;
    std::unordered_map<std::string, OpFn>  primitives;
    std::unordered_map<std::string, json>  executor_registry;  // for NEGOTIATE

    void register_primitives();
    void exec_step(const json& step, scope_t& scope);
    json resolve_arg(const json& arg, scope_t& scope);

    // SHA-256 (used by native.HASH)
    static std::string sha256_hex(const std::string& data);

    // Tiny infix expression evaluator (used by native.EVAL @expr)
    json eval_expr(const std::string& expr, scope_t& scope);
};
