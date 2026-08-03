#include "server.hpp"
#include "runtime_authority.hpp"

#define CPPHTTPLIB_NO_EXCEPTIONS 1
#include "httplib.h"

#include <iostream>

using json = nlohmann::json;

Server::Server(int port, RunFn run_fn, FileFn file_fn, GraphFn graph_fn,
               SCOFn sco_fn, DiscoveryFn discovery_fn, RunInlineFn inline_run_fn,
               FileManagerFn file_manager_fn, SidecarFn sidecar_fn)
    : port_(port), run_fn_(run_fn), inline_run_fn_(inline_run_fn),
      file_fn_(file_fn), graph_fn_(graph_fn),
      sco_fn_(sco_fn), discovery_fn_(discovery_fn),
      file_manager_fn_(file_manager_fn),
      sidecar_fn_(sidecar_fn) {}

void Server::start() {
    httplib::Server svr;

    // ── CORS helper ───────────────────────────────────────────────────────────
    auto cors = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };

    svr.Options(".*", [&](const httplib::Request&, httplib::Response& res) {
        cors(res);
        res.status = 204;
    });

    // ── POST /api/run ─────────────────────────────────────────────────────────
    svr.Post("/api/run", [this, cors](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        json body;
        if (!req.body.empty()) {
            try { body = json::parse(req.body); } catch (...) {}
        }

        // If "program" is a JSON object → inline execution
        if (!body.is_null() && body.contains("program") && body["program"].is_object()) {
            if (inline_run_fn_) {
                json result = inline_run_fn_(body["program"]);
                res.set_content(json{{"result", result}}.dump(2), "application/json");
            } else {
                res.status = 501;
                res.set_content(R"({"error":"inline programs not supported"})", "application/json");
            }
            return;
        }

        // Otherwise "program" is a file path string
        std::string path;
        if (!body.is_null()) path = body.value("program", "");
        if (path.empty())    path = req.get_param_value("program");
        if (path.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"missing program path or object\"}", "application/json");
            return;
        }
        json result = run_fn_(path);
        res.set_content(json{{"result", result}}.dump(2), "application/json");
    });

    // ── GET /api/file ─────────────────────────────────────────────────────────
    svr.Get("/api/file", [this, cors](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        std::string path = req.get_param_value("path");
        if (path.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"missing path"})", "application/json");
            return;
        }
        json result = file_fn_(path);
        res.set_content(result.dump(2), "application/json");
    });

    // ── Project/user file manager ───────────────────────────────────────────
    svr.Get("/api/file-manager", [this, cors](const httplib::Request&, httplib::Response& res) {
        cors(res);
        if (!file_manager_fn_) {
            res.status = 501;
            res.set_content(R"({"error":"file manager not configured"})", "application/json");
            return;
        }
        json result = file_manager_fn_("describe", json::object());
        res.set_content(result.dump(2), "application/json");
    });

    svr.Post(R"(/api/file-manager/(.+))", [this, cors](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        if (!file_manager_fn_) {
            res.status = 501;
            res.set_content(R"({"error":"file manager not configured"})", "application/json");
            return;
        }

        json body = json::object();
        if (!req.body.empty()) {
            try { body = json::parse(req.body); } catch (...) {
                res.status = 400;
                res.set_content(R"({"error":"invalid json body"})", "application/json");
                return;
            }
        }

        std::string verb = req.matches[1];
        json result = file_manager_fn_(verb, body);
        if (result.contains("error")) res.status = result.value("status_code", 400);
        res.set_content(result.dump(2), "application/json");
    });

    // ── Sidecar executable bootstrap contracts ──────────────────────────────
    svr.Get("/api/sidecars", [this, cors](const httplib::Request&, httplib::Response& res) {
        cors(res);
        if (!sidecar_fn_) {
            res.status = 501;
            res.set_content(R"({"error":"sidecar api not configured"})", "application/json");
            return;
        }
        json result = sidecar_fn_("list", "", "", json::object());
        res.set_content(result.dump(2), "application/json");
    });

    svr.Get(R"(/api/sidecars/([^/]+))", [this, cors](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        if (!sidecar_fn_) {
            res.status = 501;
            res.set_content(R"({"error":"sidecar api not configured"})", "application/json");
            return;
        }
        std::string name = req.matches[1];
        json result = sidecar_fn_("describe", name, "", json::object());
        if (result.contains("error")) res.status = result.value("status_code", 400);
        res.set_content(result.dump(2), "application/json");
    });

    svr.Post(R"(/api/sidecars/([^/]+)/call/([^/]+))", [this, cors](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        if (!sidecar_fn_) {
            res.status = 501;
            res.set_content(R"({"error":"sidecar api not configured"})", "application/json");
            return;
        }

        json body = json::object();
        if (!req.body.empty()) {
            try { body = json::parse(req.body); } catch (...) {
                res.status = 400;
                res.set_content(R"({"error":"invalid json body"})", "application/json");
                return;
            }
        }

        std::string name = req.matches[1];
        std::string op = req.matches[2];
        json result = sidecar_fn_("call", name, op, body);
        if (result.contains("error")) res.status = result.value("status_code", 400);
        if (result.contains("result") && result["result"].is_object() && result["result"].contains("error")) {
            res.status = result["result"].value("status_code", 400);
        }
        res.set_content(result.dump(2), "application/json");
    });

    // ── GET /api/graph ────────────────────────────────────────────────────────
    svr.Get("/api/graph", [this, cors](const httplib::Request&, httplib::Response& res) {
        cors(res);
        json result = graph_fn_();
        res.set_content(json{{"ops", result}}.dump(2), "application/json");
    });

    // ── GET /api/phases ──────────────────────────────────────────────────────
    svr.Get("/api/phases", [cors](const httplib::Request&, httplib::Response& res) {
        cors(res);
        res.set_content(RuntimeAuthority::phase_contract().dump(2), "application/json");
    });

    // ── GET /api/sco/:alias ───────────────────────────────────────────────────
    // Returns SCO object for a given alias
    svr.Get("/api/sco/(.*)", [this, cors](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        std::string alias = req.matches[1];
        if (alias.empty()) alias = req.get_param_value("alias");
        json result = sco_fn_(alias);
        if (result.contains("error")) res.status = 404;
        res.set_content(result.dump(2), "application/json");
    });

    // ── GET /api/discovery ────────────────────────────────────────────────────
    svr.Get("/api/discovery", [this, cors](const httplib::Request&, httplib::Response& res) {
        cors(res);
        json result = discovery_fn_();
        res.set_content(result.dump(2), "application/json");
    });

    // ── GET /api/health ───────────────────────────────────────────────────────
    svr.Get("/api/health", [cors](const httplib::Request&, httplib::Response& res) {
        cors(res);
        res.set_content(
            R"({"status":"ok","runtime":"json-runtime","version":"2.0.0"})",
            "application/json");
    });

    // ── GET /.well-known/sco ─────────────────────────────────────────────────
    // SCO identity endpoint (discovery protocol)
    svr.Get("/.well-known/sco", [this, cors](const httplib::Request&, httplib::Response& res) {
        cors(res);
        json identity = json{
            {"@sco", json{
                {"id",           "node.json-runtime"},
                {"type",         "runtime"},
                {"capabilities", json::array({"xcfe.run","sco.resolve","discovery"})},
                {"routes",       json::array({"sco://json-runtime"})},
                {"health",       "/api/health"}
            }}
        };
        res.set_content(identity.dump(2), "application/json");
    });

    std::cout << "[server] REST API -- port " << port_ << "\n";
    std::cout << "[server]   POST /api/run      { program: path }\n";
    std::cout << "[server]   GET  /api/file?path=<path>  (relative to @server.http_file_path)\n";
    std::cout << "[server]   GET  /api/file-manager\n";
    std::cout << "[server]   POST /api/file-manager/<verb>\n";
    std::cout << "[server]   GET  /api/sidecars\n";
    std::cout << "[server]   GET  /api/sidecars/<name>\n";
    std::cout << "[server]   POST /api/sidecars/<name>/call/<op>\n";
    std::cout << "[server]   GET  /api/graph\n";
    std::cout << "[server]   GET  /api/phases\n";
    std::cout << "[server]   GET  /api/sco/<alias>\n";
    std::cout << "[server]   GET  /api/discovery\n";
    std::cout << "[server]   GET  /api/health\n";
    std::cout << "[server]   GET  /.well-known/sco\n";
    std::cout << "[server] listening...\n";

    svr.listen("127.0.0.1", port_);
}
