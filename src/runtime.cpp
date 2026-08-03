#include "runtime.hpp"
#include "runtime_authority.hpp"
#include "server.hpp"
#include "file_system.hpp"
#include "bots.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────

static std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

static std::string safe_segment(std::string value, const std::string& fallback) {
    if (value.empty()) return fallback;
    for (char& ch : value) {
        bool ok = std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.';
        if (!ok) ch = '_';
    }
    if (value == "." || value == "..") return fallback;
    return value;
}

static std::string slurp_text(const fs::path& path, std::uintmax_t max_bytes) {
    if (!fs::exists(path)) throw std::runtime_error("file not found");
    if (!fs::is_regular_file(path)) throw std::runtime_error("not a file");
    auto size = fs::file_size(path);
    if (size > max_bytes) throw std::runtime_error("file too large");

    std::ifstream f(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

static bool is_under_root(const fs::path& root, const fs::path& candidate) {
    std::string root_s = lower_copy(root.lexically_normal().string());
    std::string cand_s = lower_copy(candidate.lexically_normal().string());
    if (!root_s.empty() && root_s.back() != '\\' && root_s.back() != '/') root_s.push_back('\\');
    return cand_s == root_s.substr(0, root_s.size() - 1) || cand_s.rfind(root_s, 0) == 0;
}

bool Runtime::load_manifest(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    try { f >> manifest; } catch (const json::exception& e) {
        std::cerr << "[runtime] manifest error: " << e.what() << "\n";
        return false;
    }
    fs::path manifest_path = fs::absolute(fs::path(path)).lexically_normal();
    manifest_dir_ = manifest_path.parent_path().string();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────

void Runtime::set_port_override(int port) {
    port_override_ = port;
}

void Runtime::add_sidecar_path(const std::string& path) {
    if (!path.empty()) sidecar_overrides_.push_back(path);
}

int Runtime::resolve_server_port() const {
    if (port_override_ > 0) return port_override_;

    if (const char* env_port = std::getenv("ASX_JSON_RUNTIME_PORT")) {
        try { return std::stoi(env_port); } catch (...) {}
    }

    if (manifest.contains("@server")) {
        std::string port_ref = manifest["@server"].value("port_ref", "");
        if (!port_ref.empty() && manifest.contains("@ports") && manifest["@ports"].contains(port_ref)) {
            return manifest["@ports"][port_ref].value("port", 8787);
        }
        if (manifest["@server"].contains("port")) {
            return manifest["@server"].value("port", 8787);
        }
    }

    if (manifest.contains("@ports") && manifest["@ports"].contains("json_runtime_object_server")) {
        return manifest["@ports"]["json_runtime_object_server"].value("port", 8787);
    }

    return 8787;
}

std::string Runtime::resolve_manifest_path(const std::string& path) const {
    fs::path p(path);
    if (p.is_absolute()) return p.lexically_normal().string();
    return (fs::path(manifest_dir_) / p).lexically_normal().string();
}

std::string Runtime::resolve_host_directory() const {
    std::string host_directory = ".";

    if (manifest.contains("@paths") && manifest["@paths"].is_object() &&
        manifest["@paths"].contains("host_directory") &&
        manifest["@paths"]["host_directory"].is_string()) {
        host_directory = manifest["@paths"]["host_directory"].get<std::string>();
    }

    if (manifest.contains("@server") && manifest["@server"].is_object() &&
        manifest["@server"].contains("host_directory") &&
        manifest["@server"]["host_directory"].is_string()) {
        host_directory = manifest["@server"]["host_directory"].get<std::string>();
    }

    if (host_directory == "$manifest_directory") {
        return fs::absolute(fs::path(manifest_dir_)).lexically_normal().string();
    }
    if (host_directory == "$root") {
        std::string root = ".";
        if (manifest.contains("@paths") && manifest["@paths"].is_object() &&
            manifest["@paths"].contains("root") && manifest["@paths"]["root"].is_string()) {
            root = manifest["@paths"]["root"].get<std::string>();
        }
        return fs::absolute(fs::path(resolve_manifest_path(root))).lexically_normal().string();
    }

    return fs::absolute(fs::path(resolve_manifest_path(host_directory))).lexically_normal().string();
}

std::string Runtime::resolve_http_file_root() const {
    std::string http_file_path = "$host_directory";

    if (manifest.contains("@server") && manifest["@server"].is_object() &&
        manifest["@server"].contains("http_file_path") &&
        manifest["@server"]["http_file_path"].is_string()) {
        http_file_path = manifest["@server"]["http_file_path"].get<std::string>();
    }

    if (http_file_path == "$host_directory") return resolve_host_directory();
    if (http_file_path == "$manifest_directory") {
        return fs::absolute(fs::path(manifest_dir_)).lexically_normal().string();
    }
    if (http_file_path == "$root") {
        std::string root = ".";
        if (manifest.contains("@paths") && manifest["@paths"].is_object() &&
            manifest["@paths"].contains("root") && manifest["@paths"]["root"].is_string()) {
            root = manifest["@paths"]["root"].get<std::string>();
        }
        return fs::absolute(fs::path(resolve_manifest_path(root))).lexically_normal().string();
    }

    return fs::absolute(fs::path(resolve_manifest_path(http_file_path))).lexically_normal().string();
}

std::string Runtime::resolve_http_file_path(const std::string& path) const {
    if (path.empty() || path.find('\0') != std::string::npos || fs::path(path).is_absolute()) {
        throw std::runtime_error("path_rejected");
    }

    fs::path root = fs::absolute(fs::path(resolve_http_file_root())).lexically_normal();
    fs::path target = (root / fs::path(path)).lexically_normal();
    if (!is_under_root(root, target)) {
        throw std::runtime_error("path_rejected");
    }
    return target.string();
}

json Runtime::execute_file_manager(const std::string& verb, const json& body) {
    const std::uintmax_t max_bytes =
        manifest.contains("@file_manager")
            ? manifest["@file_manager"].value("max_file_size", 1048576)
            : 1048576;

    std::string session_root = "user_sessions";
    if (manifest.contains("@file_manager")) {
        session_root = manifest["@file_manager"].value("session_root", session_root);
    }

    fs::path root = fs::absolute(fs::path(resolve_manifest_path(session_root))).lexically_normal();
    std::string fallback_user = body.value("user", std::string("default"));
    std::string fallback_project = body.value("project", std::string("default"));
    std::string user = safe_segment(body.value("user_id", fallback_user), "default");
    std::string project = safe_segment(body.value("project_id", fallback_project), "default");
    std::string rel = body.value("path", std::string("."));
    if (rel.empty()) rel = ".";
    if (rel.find('\0') != std::string::npos || fs::path(rel).is_absolute()) {
        return json{{"error","path_rejected"},{"status_code",400}};
    }

    fs::path project_root = (root / user / project).lexically_normal();
    fs::path target = (project_root / fs::path(rel)).lexically_normal();
    if (!is_under_root(project_root, target)) {
        return json{{"error","path_rejected"},{"status_code",400}};
    }

    try {
        if (verb == "describe") {
            return json{
                {"@kind", "json_runtime.file_manager.v1"},
                {"scope", "project_user_session"},
                {"session_root", root.string()},
                {"verbs", json::array({"init","list","read","write","patch","stat","search"})},
                {"required", json::array({"user_id","project_id"})},
                {"path_rule", "relative paths only; resolved under session_root/user_id/project_id"}
            };
        }

        if (verb == "init") {
            fs::create_directories(project_root / "src");
            fs::create_directories(project_root / "tests");
            fs::create_directories(project_root / "docs");
            fs::create_directories(project_root / "config");
            return json{{"ok",true},{"user_id",user},{"project_id",project},{"root",project_root.string()}};
        }

        if (verb == "list") {
            fs::create_directories(target);
            json items = json::array();
            for (const auto& entry : fs::directory_iterator(target)) {
                items.push_back(json{
                    {"name", entry.path().filename().string()},
                    {"path", fs::relative(entry.path(), project_root).generic_string()},
                    {"type", entry.is_directory() ? "directory" : "file"},
                    {"size", entry.is_regular_file() ? static_cast<std::uintmax_t>(entry.file_size()) : 0}
                });
            }
            return json{{"ok",true},{"user_id",user},{"project_id",project},{"path",rel},{"items",items}};
        }

        if (verb == "read") {
            return json{{"ok",true},{"user_id",user},{"project_id",project},{"path",rel},{"content",slurp_text(target, max_bytes)}};
        }

        if (verb == "write") {
            std::string content = body.value("content", std::string(""));
            if (content.size() > max_bytes) return json{{"error","content too large"},{"status_code",413}};
            fs::create_directories(target.parent_path());
            std::ofstream out(target, std::ios::binary);
            out << content;
            return json{{"ok",true},{"user_id",user},{"project_id",project},{"path",rel},{"bytes",content.size()}};
        }

        if (verb == "patch") {
            std::string pattern = body.value("pattern", std::string(""));
            std::string replacement = body.value("replacement", std::string(""));
            if (pattern.empty()) return json{{"error","missing pattern"},{"status_code",400}};
            std::string content = slurp_text(target, max_bytes);
            size_t pos = content.find(pattern);
            if (pos == std::string::npos) return json{{"error","pattern not found"},{"status_code",404}};
            content.replace(pos, pattern.size(), replacement);
            std::ofstream out(target, std::ios::binary);
            out << content;
            return json{{"ok",true},{"user_id",user},{"project_id",project},{"path",rel},{"patched",true}};
        }

        if (verb == "stat") {
            if (!fs::exists(target)) return json{{"error","not found"},{"status_code",404}};
            return json{
                {"ok", true},
                {"user_id", user},
                {"project_id", project},
                {"path", rel},
                {"type", fs::is_directory(target) ? "directory" : "file"},
                {"size", fs::is_regular_file(target) ? static_cast<std::uintmax_t>(fs::file_size(target)) : 0}
            };
        }

        if (verb == "search") {
            std::string query = body.value("query", std::string(""));
            if (query.empty()) return json{{"error","missing query"},{"status_code",400}};
            json matches = json::array();
            fs::path start = fs::exists(target) && fs::is_directory(target) ? target : project_root;
            for (const auto& entry : fs::recursive_directory_iterator(start)) {
                if (!entry.is_regular_file()) continue;
                if (entry.file_size() > max_bytes) continue;
                std::string content = slurp_text(entry.path(), max_bytes);
                size_t pos = content.find(query);
                if (pos != std::string::npos) {
                    matches.push_back(json{
                        {"path", fs::relative(entry.path(), project_root).generic_string()},
                        {"offset", pos}
                    });
                }
            }
            return json{{"ok",true},{"user_id",user},{"project_id",project},{"query",query},{"matches",matches}};
        }

        return json{{"error","unknown file-manager verb"},{"verb",verb},{"status_code",404}};
    } catch (const std::exception& e) {
        return json{{"error",e.what()},{"status_code",500}};
    }
}

bool Runtime::load_declared_sidecar(const std::string& name) {
    if (name.empty() || !manifest.contains("@sidecars") || !manifest["@sidecars"].is_object()) return false;
    if (!name.empty() && name[0] == '_') return false;
    if (!manifest["@sidecars"].contains(name)) return false;
    const json& entry = manifest["@sidecars"][name];
    if (!entry.is_object()) return false;

    std::string path = resolve_manifest_path(entry.value("path", "sco/sidecars/" + name + ".json"));
    if (!sidecars.load(path, name, stdlib_ops.is_null() ? nullptr : &stdlib_ops)) return false;

    const Sidecar* s = sidecars.get(name);
    if (s && s->op_defs.is_object()) xcfe.load_ops(s->op_defs);
    return true;
}

json Runtime::execute_sidecar_api(const std::string& action, const std::string& name,
                                  const std::string& op, const json& body) {
    json phase_contract = RuntimeAuthority::phase_contract();

    if (action == "list") {
        json declared = json::array();
        if (manifest.contains("@sidecars") && manifest["@sidecars"].is_object()) {
            for (auto& [decl_name, entry] : manifest["@sidecars"].items()) {
                if (!decl_name.empty() && decl_name[0] == '_') continue;
                if (!entry.is_object()) continue;
                declared.push_back(json{
                    {"name", decl_name},
                    {"path", entry.value("path", "")},
                    {"loaded", sidecars.has(decl_name)}
                });
            }
        }

        json loaded = json::array();
        for (const auto& loaded_name : sidecars.list()) loaded.push_back(loaded_name);

        json contract = RuntimeAuthority::app_contract(manifest);

        return json{
            {"@kind", "json_runtime.sidecar.bootstrap.v1"},
            {"rule", "A sidecar JSON is a bootstrap executable contract for apps, games, programs, websites, schemas, and tools. JSON declares; XCFE/KUHUL executes."},
            {"app_sidecar_pattern", "*.manifest.json"},
            {"examples", RuntimeAuthority::sidecar_examples()},
            {"launch", "./json_runtime.exe --manifest manifest.json --sidecar <bundle>.manifest.json"},
            {"contract", contract},
            {"declared", declared},
            {"loaded", loaded},
            {"routes", json::array({
                "GET /api/sidecars",
                "GET /api/sidecars/<name>",
                "POST /api/sidecars/<name>/call/<op>"
            })},
            {"phase_contract", phase_contract}
        };
    }

    if (action == "describe") {
        if (name.empty()) return execute_sidecar_api("list", "", "", body);
        if (!sidecars.has(name)) {
            json declared = nullptr;
            if (!name.empty() && name[0] != '_' &&
                manifest.contains("@sidecars") && manifest["@sidecars"].is_object() &&
                manifest["@sidecars"].contains(name) && manifest["@sidecars"][name].is_object()) {
                declared = manifest["@sidecars"][name];
            }
            return json{
                {"@kind", "json_runtime.sidecar.bootstrap.v1"},
                {"name", name},
                {"loaded", false},
                {"declared", declared},
                {"phase_contract", phase_contract}
            };
        }

        const Sidecar* s = sidecars.get(name);
        json ops = json::array();
        if (s && s->op_defs.is_object()) {
            for (auto& [op_name, _] : s->op_defs.items()) ops.push_back(op_name);
        }
        json cap = s ? s->capability : json::object();
        bool bot_contract = BotRuntime::is_bot_manifest(cap);
        std::string role = RuntimeAuthority::classify_sidecar(cap);
        return json{
            {"@kind", "json_runtime.sidecar.bootstrap.v1"},
            {"name", name},
            {"loaded", true},
            {"role", role},
            {"capability", cap},
            {"bot_contract", bot_contract ? BotRuntime::contract() : json(nullptr)},
            {"ops", ops},
            {"phase_contract", phase_contract}
        };
    }

    if (action == "call") {
        if (name.empty()) return json{{"error","missing sidecar name"},{"status_code",400}};
        if (op.empty()) return json{{"error","missing sidecar op"},{"status_code",400}};
        if (!sidecars.has(name) && !load_declared_sidecar(name)) {
            return json{{"error","sidecar_not_found"},{"name",name},{"status_code",404}};
        }

        scope_t scope;
        json input = body;
        if (body.is_object() && body.contains("scope") && body["scope"].is_object()) {
            input = body["scope"];
        }
        if (input.is_object()) {
            for (auto& [k, v] : input.items()) scope[k] = v;
        }

        json result = sidecars.call(name, op, scope);
        if (result.contains("error")) result["status_code"] = result.value("status_code", 400);
        return json{
            {"@kind", "json_runtime.sidecar.call_result.v1"},
            {"sidecar", name},
            {"op", op},
            {"result", result},
            {"phase", "sek"}
        };
    }

    return json{{"error","unknown sidecar action"},{"action",action},{"status_code",404}};
}

bool Runtime::load_stdlib() {
    std::string stdlib_path = "programs/stdlib.json";
    if (manifest.contains("@paths") && manifest["@paths"].contains("programs")) {
        stdlib_path = manifest["@paths"]["programs"].get<std::string>() + "stdlib.json";
    }
    try {
        json stdlib = FileSystem::load_json(resolve_manifest_path(stdlib_path));
        if (stdlib.contains("@ops")) {
            stdlib_ops = stdlib["@ops"];
            xcfe.load_ops(stdlib_ops);
            std::cout << "[runtime] stdlib loaded: " << stdlib_ops.size() << " ops\n";
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[runtime] stdlib warning: " << e.what() << "\n";
        return false;
    }
}

void Runtime::load_sco_registry() {
    std::string reg_path = "sco/registry.json";
    if (manifest.contains("@sco") && manifest["@sco"].contains("registry"))
        reg_path = manifest["@sco"]["registry"].get<std::string>();

    if (sco.load(resolve_manifest_path(reg_path))) {
        // Wire SCO registry into XCFE
        xcfe.sco = &sco;
    } else {
        std::cout << "[runtime] no SCO registry (optional)\n";
        xcfe.sco = &sco;  // still wire empty registry
    }
}

void Runtime::load_sidecars() {
    xcfe.sidecars = &sidecars;

    int override_index = 0;
    for (const auto& path : sidecar_overrides_) {
        fs::path p(path);
        std::string name = p.stem().string();
        if (name.empty()) name = "sidecar_" + std::to_string(++override_index);
        std::cout << "[runtime] loading explicit sidecar: " << path << "\n";
        if (sidecars.load(resolve_manifest_path(path), name, stdlib_ops.is_null() ? nullptr : &stdlib_ops)) {
            const Sidecar* s = sidecars.get(name);
            if (s && !s->op_defs.is_null() && s->op_defs.is_object()) xcfe.load_ops(s->op_defs);
        }
    }

    if (!manifest.contains("@sidecars")) return;
    std::string load_policy = manifest["@sidecars"].value("_load_policy", std::string("lazy"));
    if (load_policy != "eager") {
        std::cout << "[runtime] sidecars registered lazy; eager load skipped\n";
        return;
    }
    for (auto& [name, entry] : manifest["@sidecars"].items()) {
        if (!name.empty() && name[0] == '_') continue;
        if (!entry.is_object()) continue;
        std::string path = resolve_manifest_path(entry.value("path", "sco/sidecars/" + name + ".json"));
        sidecars.load(path, name, stdlib_ops.is_null() ? nullptr : &stdlib_ops);
        // Merge sidecar ops into main XCFE registry
        const Sidecar* s = sidecars.get(name);
        if (s && !s->op_defs.is_null()) xcfe.load_ops(s->op_defs);
    }
}

void Runtime::run_discovery() {
    bool disc_enabled = manifest.contains("@discovery") &&
                        manifest["@discovery"].value("enabled", false);
    if (!disc_enabled) return;

    Discovery d;
    auto ports = Discovery::default_ports();
    if (manifest["@discovery"].contains("ports")) {
        ports.clear();
        for (auto& p : manifest["@discovery"]["ports"]) ports.push_back(p.get<int>());
    }

    auto services = d.scan(ports);
    for (auto& svc : services) {
        // Register into SCO registry as API endpoints
        sco.register_api(svc.sco_alias, "http://127.0.0.1:" + std::to_string(svc.port));
        sco.register_api(svc.service_name, "http://127.0.0.1:" + std::to_string(svc.port));
    }

    if (!services.empty()) {
        std::cout << "[runtime] discovery: " << services.size() << " services registered\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void Runtime::boot() {
    // 1. Load stdlib (JSON-defined ops)
    load_stdlib();

    // 2. Load SCO registry
    load_sco_registry();

    // 3. Load sidecars
    load_sidecars();

    // 4. Auto-discovery (user-gated)
    run_discovery();

    // 5. Execute entry program
    std::string entry = manifest.value("@entry", "programs/main.json");
    std::cout << "[runtime] boot entry: " << entry << "\n";
    try {
        json program = FileSystem::load_json(resolve_manifest_path(entry));
        xcfe.execute(program);
    } catch (const std::exception& e) {
        std::cerr << "[runtime] entry error: " << e.what() << "\n";
    }

    // 6. Start REST server (if enabled)
    bool server_enabled = manifest.contains("@server") &&
                          manifest["@server"].value("enabled", false);
    if (!server_enabled) return;

    int port = resolve_server_port();

    // RunFn: fresh XCFE per request (stateless), shares SCO + sidecars
    auto run_fn = [this](const std::string& path) -> json {
        XCFE req_xcfe;
        req_xcfe.sco      = &sco;
        req_xcfe.sidecars = &sidecars;
        if (!stdlib_ops.is_null()) req_xcfe.load_ops(stdlib_ops);
        try {
            json prog = FileSystem::load_json(resolve_manifest_path(path));
            req_xcfe.execute(prog);
            json result = json::object();
            for (auto& [k, v] : req_xcfe.global_state) result[k] = v;
            return result;
        } catch (const std::exception& e) {
            return json{{"error", e.what()}};
        }
    };

    // FileFn: sandboxed JSON object read under @server.http_file_path.
    auto file_fn = [this](const std::string& path) -> json {
        try { return FileSystem::load_json(resolve_http_file_path(path)); }
        catch (const std::exception& e) { return json{{"error", e.what()}}; }
    };

    // GraphFn: op registry
    auto graph_fn = [this]() -> json {
        json ops = json::object();
        for (auto& name : xcfe.list_ops()) ops[name] = true;
        return ops;
    };

    // SCO fn: resolve alias → descriptor
    auto sco_fn = [this](const std::string& alias) -> json {
        SCOObject obj;
        if (sco.resolve(alias, obj)) {
            json out = json{{"alias", obj.alias}, {"type", obj.type}, {"sha256", obj.sha256}};
            if (!obj.endpoint.empty()) out["endpoint"] = obj.endpoint;
            if (!obj.data.is_null()) out["data"] = obj.data;
            return out;
        }
        return json{{"error","not_found"},{"alias",alias}};
    };

    // Discovery fn: return cached discovered services
    auto discovery_fn = [this]() -> json {
        json result = json::array();
        for (auto& alias : sco.list_aliases()) {
            if (alias.find("discovered_") == 0) {
                SCOObject obj;
                if (sco.resolve(alias, obj))
                    result.push_back(json{{"alias",alias},{"endpoint",obj.endpoint}});
            }
        }
        return result;
    };

    // Inline run fn: executes an inline JSON program object (no file load)
    auto inline_run_fn = [this](const json& program) -> json {
        XCFE req_xcfe;
        req_xcfe.sco      = &sco;
        req_xcfe.sidecars = &sidecars;
        if (!stdlib_ops.is_null()) req_xcfe.load_ops(stdlib_ops);
        try {
            req_xcfe.execute(program);
            json result = json::object();
            for (auto& [k, v] : req_xcfe.global_state) result[k] = v;
            return result;
        } catch (const std::exception& e) {
            return json{{"error", e.what()}};
        }
    };

    auto file_manager_fn = [this](const std::string& verb, const json& body) -> json {
        return execute_file_manager(verb, body);
    };

    auto sidecar_fn = [this](const std::string& action, const std::string& name,
                             const std::string& op, const json& body) -> json {
        return execute_sidecar_api(action, name, op, body);
    };

    Server server(port, run_fn, file_fn, graph_fn, sco_fn, discovery_fn,
                  inline_run_fn, file_manager_fn, sidecar_fn);
    server.start();  // blocks
}
