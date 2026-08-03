/**
 * sco_main.cpp -- SCO Runtime CLI
 *
 * Usage:
 *   sco_runtime.exe [registry.json] [--port N]   # boot REST server
 *   sco_runtime.exe --resolve <alias>            # resolve alias → print object
 *   sco_runtime.exe --list                       # list all registered aliases
 *   sco_runtime.exe --run <sco_alias>            # execute a program-type SCO object
 *   sco_runtime.exe --hash <file.json>           # compute SHA-256 of a JSON file
 *   sco_runtime.exe --register <alias> <file>    # register file into registry
 */

#include "sco.hpp"
#include "xcfe.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::json;

static const int DEFAULT_SCO_SERVER_PORT = 6002;
static const std::string DEFAULT_REGISTRY = "sco/registry.json";

// ── helpers ──────────────────────────────────────────────────────────────────

static json load_json_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open: " + path);
    return json::parse(f);
}

static void print_usage() {
    std::cout <<
        "sco_runtime -- SCO Registry & Execution Engine\n\n"
        "  sco_runtime [registry.json] [--port N]  boot REST server\n"
        "  sco_runtime --resolve <alias>            resolve alias, print JSON object\n"
        "  sco_runtime --list [registry.json]       list all aliases\n"
        "  sco_runtime --run <alias> [registry]     execute a program SCO\n"
        "  sco_runtime --hash <file.json>           SHA-256 of file\n"
        "  sco_runtime --register <alias> <file> [registry]\n"
        "                                           register file into registry\n";
}

static int resolve_sco_port(int override_port) {
    if (override_port > 0) return override_port;
    if (const char* env_port = std::getenv("ASX_SCO_RUNTIME_PORT")) {
        try { return std::stoi(env_port); } catch (...) {}
    }
    return DEFAULT_SCO_SERVER_PORT;
}

// ── command handlers ──────────────────────────────────────────────────────────

static int cmd_list(const std::string& reg_path) {
    SCORegistry reg;
    if (!reg.load(reg_path)) {
        std::cerr << "[sco] failed to load registry: " << reg_path << "\n";
        return 1;
    }
    auto aliases = reg.list_aliases();
    std::cout << "Aliases in " << reg_path << " (" << aliases.size() << "):\n";
    for (const auto& a : aliases) {
        SCOObject obj;
        reg.resolve(a, obj);
        std::cout << "  " << a << "  [" << obj.type << "]";
        if (!obj.endpoint.empty()) std::cout << "  → " << obj.endpoint;
        if (!obj.path.empty())     std::cout << "  @ " << obj.path;
        std::cout << "\n";
    }
    return 0;
}

static int cmd_resolve(const std::string& alias, const std::string& reg_path) {
    SCORegistry reg;
    if (!reg.load(reg_path)) {
        std::cerr << "[sco] failed to load registry: " << reg_path << "\n";
        return 1;
    }
    SCOObject obj;
    if (!reg.resolve(alias, obj)) {
        std::cerr << "[sco] alias not found: " << alias << "\n";
        return 1;
    }
    json out;
    out["alias"]    = obj.alias;
    out["type"]     = obj.type;
    out["sha256"]   = obj.sha256;
    out["path"]     = obj.path;
    out["endpoint"] = obj.endpoint;
    if (!obj.data.is_null()) out["data"] = obj.data;
    std::cout << out.dump(2) << "\n";
    return 0;
}

static int cmd_run(const std::string& alias, const std::string& reg_path) {
    SCORegistry reg;
    if (!reg.load(reg_path)) {
        std::cerr << "[sco] failed to load registry: " << reg_path << "\n";
        return 1;
    }
    SCOObject obj;
    if (!reg.resolve(alias, obj)) {
        std::cerr << "[sco] alias not found: " << alias << "\n";
        return 1;
    }
    if (obj.type != "program" && obj.type != "agent") {
        std::cerr << "[sco] alias '" << alias << "' is type '" << obj.type
                  << "' -- only program/agent types are executable\n";
        return 1;
    }
    if (obj.data.is_null()) {
        std::cerr << "[sco] no data loaded for alias: " << alias << "\n";
        return 1;
    }
    XCFE xcfe;
    xcfe.sco = &reg;
    // load stdlib if available
    try {
        auto stdlib = load_json_file("programs/stdlib.json");
        if (stdlib.contains("@ops")) xcfe.load_ops(stdlib["@ops"]);
    } catch (...) {}
    xcfe.execute(obj.data);
    std::cout << "[sco] execution complete\n";
    return 0;
}

static int cmd_hash(const std::string& file_path) {
    try {
        auto j = load_json_file(file_path);
        std::cout << SCORegistry::hash_json(j) << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[sco] " << e.what() << "\n";
        return 1;
    }
    return 0;
}

static int cmd_register(const std::string& alias, const std::string& file,
                        const std::string& reg_path) {
    // Load existing registry or start fresh
    SCORegistry reg;
    reg.load(reg_path); // ok if missing

    json data;
    try { data = load_json_file(file); }
    catch (const std::exception& e) {
        std::cerr << "[sco] " << e.what() << "\n";
        return 1;
    }

    SCOObject obj;
    obj.alias  = alias;
    obj.type   = data.value("@type", "program");
    obj.path   = file;
    obj.sha256 = SCORegistry::hash_json(data);
    obj.data   = data;
    reg.register_object(alias, obj);

    // Write updated registry
    json reg_json = reg.to_json();
    std::ofstream out(reg_path);
    if (!out) {
        std::cerr << "[sco] cannot write registry: " << reg_path << "\n";
        return 1;
    }
    out << reg_json.dump(2) << "\n";
    std::cout << "[sco] registered '" << alias << "'  sha256=" << obj.sha256 << "\n";
    return 0;
}

// ── REST server mode ──────────────────────────────────────────────────────────

static int run_server(const std::string& reg_path, int port) {
    SCORegistry reg;
    if (!reg.load(reg_path)) {
        std::cerr << "[sco] failed to load registry: " << reg_path << "\n";
        return 1;
    }

    httplib::Server svr;

    // CORS helper
    auto cors = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };

    // GET /api/sco/:alias
    svr.Get(R"(/api/sco/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        std::string alias = req.matches[1];
        SCOObject obj;
        if (!reg.resolve(alias, obj)) {
            res.status = 404;
            res.set_content("{\"error\":\"not found\"}", "application/json");
            return;
        }
        json j;
        j["alias"]    = obj.alias;
        j["type"]     = obj.type;
        j["sha256"]   = obj.sha256;
        j["path"]     = obj.path;
        j["endpoint"] = obj.endpoint;
        if (!obj.data.is_null()) j["data"] = obj.data;
        res.set_content(j.dump(), "application/json");
    });

    // GET /api/list
    svr.Get("/api/list", [&](const httplib::Request&, httplib::Response& res) {
        cors(res);
        auto aliases = reg.list_aliases();
        json arr = json::array();
        for (const auto& a : aliases) {
        SCOObject obj;
            reg.resolve(a, obj);
            arr.push_back({{"alias", a}, {"type", obj.type}});
        }
        res.set_content(arr.dump(), "application/json");
    });

    // POST /api/sco/:alias  -- register new object
    svr.Post(R"(/api/sco/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        std::string alias = req.matches[1];
        try {
            json data = json::parse(req.body);
            SCOObject obj;
            obj.alias  = alias;
            obj.type   = data.value("@type", "program");
            obj.sha256 = SCORegistry::hash_json(data);
            obj.data   = data;
            reg.register_object(alias, obj);
            res.set_content("{\"ok\":true,\"sha256\":\"" + obj.sha256 + "\"}", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
        }
    });

    // GET /.well-known/sco  -- identity descriptor
    svr.Get("/.well-known/sco", [](const httplib::Request&, httplib::Response& res) {
        json id = {
            {"service", "sco_runtime"},
            {"version", "2.0"},
            {"capabilities", json::array({"sco.resolve", "sco.register", "sco.list"})}
        };
        res.set_content(id.dump(), "application/json");
    });

    // OPTIONS (CORS preflight)
    svr.Options(".*", [&](const httplib::Request&, httplib::Response& res) { cors(res); });

    std::cout << "[sco_runtime] registry: " << reg_path << "\n";
    std::cout << "[sco_runtime] aliases:  " << reg.list_aliases().size() << "\n";
    std::cout << "[sco_runtime] listening on :" << port << "\n";
    svr.listen("0.0.0.0", port);
    return 0;
}

// ── entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    int port_override = 0;

    for (size_t i = 0; i < args.size();) {
        if ((args[i] == "--port" || args[i] == "-p") && i + 1 < args.size()) {
            port_override = std::stoi(args[i + 1]);
            args.erase(args.begin() + i, args.begin() + i + 2);
        } else if (args[i].rfind("--port=", 0) == 0) {
            port_override = std::stoi(args[i].substr(7));
            args.erase(args.begin() + i);
        } else {
            ++i;
        }
    }

    int server_port = resolve_sco_port(port_override);

    if (args.empty()) {
        return run_server(DEFAULT_REGISTRY, server_port);
    }

    if (args[0] == "--help" || args[0] == "-h") {
        print_usage();
        return 0;
    }

    if (args[0] == "--list") {
        std::string reg = args.size() > 1 ? args[1] : DEFAULT_REGISTRY;
        return cmd_list(reg);
    }

    if (args[0] == "--resolve" && args.size() >= 2) {
        std::string reg = args.size() > 2 ? args[2] : DEFAULT_REGISTRY;
        return cmd_resolve(args[1], reg);
    }

    if (args[0] == "--run" && args.size() >= 2) {
        std::string reg = args.size() > 2 ? args[2] : DEFAULT_REGISTRY;
        return cmd_run(args[1], reg);
    }

    if (args[0] == "--hash" && args.size() >= 2) {
        return cmd_hash(args[1]);
    }

    if (args[0] == "--register" && args.size() >= 3) {
        std::string reg = args.size() > 3 ? args[3] : DEFAULT_REGISTRY;
        return cmd_register(args[1], args[2], reg);
    }

    // positional: treat first arg as registry path, boot server
    if (args[0][0] != '-') {
        return run_server(args[0], server_port);
    }

    print_usage();
    return 1;
}
