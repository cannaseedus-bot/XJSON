#pragma once
// sco.hpp -- Symbolic Cache Object registry
//
// SCO URI scheme:  sco://<alias>   → lookup alias → sha256 → object
// Aliases live in a JSON registry file (sco/registry.json).
//
// SCO object structure (JSON):
//   {
//     "@sco": {
//       "id":      "sha256:abc...",
//       "type":    "program | data | sidecar | agent | api",
//       "payload": { ... },
//       "meta": {
//         "deps":         ["sha256:..."],
//         "capabilities": ["GPU"],
//         "version":      1
//       }
//     }
//   }
//
// Registry file (sco/registry.json):
//   {
//     "aliases": {
//       "agent_neo": { "sha256": "abc...", "path": "sco/agents/neo.json", "type": "agent" },
//       "OLLAMA_API": { "type": "api",  "endpoint": "http://localhost:11434" }
//     }
//   }

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

struct SCOObject {
    std::string alias;
    std::string sha256;
    std::string path;
    std::string type;    // program | data | sidecar | agent | api
    std::string endpoint; // for type=="api"
    json        data;    // loaded payload (null if not yet loaded)
};

class SCORegistry {
public:
    // Load alias registry from a JSON file
    bool load(const std::string& registry_path);

    // Resolve alias → SCOObject (loads payload on demand, verifies hash)
    bool resolve(const std::string& alias, SCOObject& out);

    // Register an alias programmatically (from discovery, sidecar INIT, etc.)
    void register_object(const std::string& alias, SCOObject obj);

    // Register an API endpoint alias
    void register_api(const std::string& alias, const std::string& endpoint);

    // Compute SHA-256 of JSON value (utility)
    static std::string hash_json(const json& val);

    // Export current registry as JSON
    json to_json() const;

    // List all aliases
    std::vector<std::string> list_aliases() const;

private:
    std::unordered_map<std::string, SCOObject> objects_;

    bool load_object(SCOObject& obj);          // disk → obj.data
    bool verify_hash(const SCOObject& obj);    // sha256(obj.data) == obj.sha256
};
