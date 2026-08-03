// sw.cpp -- Sidecar Store (SW). See sw.hpp.
#include "sw.hpp"

#include <fstream>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <vector>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ─── helpers ──────────────────────────────────────────────────────────────────

static bool file_exists(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFREG);
}

static std::string to_absolute(const std::string& p) {
#ifdef _WIN32
    char full[MAX_PATH];
    if (GetFullPathNameA(p.c_str(), MAX_PATH, full, nullptr)) return std::string(full);
#endif
    return p;
}

static std::string dir_of(const std::string& path) {
    auto p = path.find_last_of("/\\");
    return (p == std::string::npos) ? std::string(".") : path.substr(0, p);
}

static std::string make_temp_path() {
#ifdef _WIN32
    char dir[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, dir)) return "sw_req.json";
    char path[MAX_PATH];
    if (!GetTempFileNameA(dir, "sw", 0, path)) return std::string(dir) + "sw_req.json";
    return std::string(path);
#else
    return std::string("/tmp/sw_req_") + std::to_string(::rand()) + ".json";
#endif
}

// ─── SidecarStore ─────────────────────────────────────────────────────────────

bool SidecarStore::load(const std::string& manifest_path) {
    std::ifstream f(manifest_path);
    if (!f) { std::cerr << "[sw] cannot open " << manifest_path << "\n"; return false; }
    std::stringstream ss; ss << f.rdbuf();
    try {
        json doc = json::parse(ss.str());
        store_ = doc.value("@sidecars", json::object());
    } catch (const std::exception& e) {
        std::cerr << "[sw] parse error (" << manifest_path << "): " << e.what() << "\n";
        return false;
    }
    root_ = dir_of(manifest_path);
    return true;
}

std::string SidecarStore::resolve_bin(const json& entry) const {
    if (!entry.contains("bin") || !entry["bin"].is_string()) return "";
    const std::string bin = entry["bin"].get<std::string>();

    if (file_exists(bin)) return to_absolute(bin);           // absolute or cwd-relative
    std::string byRoot = root_ + "/" + bin;
    if (file_exists(byRoot)) return to_absolute(byRoot);     // beside the manifest

    if (entry.contains("bin_paths") && entry["bin_paths"].is_array()) {
        for (const auto& d : entry["bin_paths"]) {
            if (!d.is_string()) continue;
            const std::string dir = d.get<std::string>();
            std::string c1 = dir + "/" + bin;               // relative to cwd
            if (file_exists(c1)) return to_absolute(c1);
            std::string c2 = root_ + "/" + dir + "/" + bin;  // relative to manifest
            if (file_exists(c2)) return to_absolute(c2);
        }
    }
    return "";
}

json SidecarStore::call_external(const std::string& bin, const std::string& op, const json& body) const {
    json req = body.is_object() ? body : json::object();
    req["operation"] = op;

    // Write the request to a temp file and feed it via stdin redirection. This is
    // bidirectional-safe (unlike a one-way _popen write pipe) and sidesteps shell
    // quoting of the JSON payload.
    const std::string tmp = make_temp_path();
    { std::ofstream tf(tmp); tf << req.dump(); }

    std::string exe = bin;
#ifdef _WIN32
    for (char& c : exe) if (c == '/') c = '\\';  // cmd.exe splits on '/'
    // Wrap the whole command in an extra quote pair: `cmd /c` strips the outer
    // pair (the command has >2 quotes and a '<'), leaving the inner quotes intact.
    std::string cmd = "\"\"" + exe + "\" < \"" + tmp + "\"\"";
#else
    std::string cmd = "\"" + exe + "\" < \"" + tmp + "\"";
#endif
    std::string out;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (pipe) {
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) out.append(buf, n);
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
    }
    std::remove(tmp.c_str());

    try {
        return json::parse(out);
    } catch (...) {
        return json{{"status", "error"}, {"error", "sidecar produced non-JSON output"},
                    {"bin", bin}, {"op", op}, {"raw", out}};
    }
}

json SidecarStore::list() const {
    json arr = json::array();
    for (auto it = store_.begin(); it != store_.end(); ++it) {
        const json& e = it.value();
        json row;
        row["name"]      = it.key();
        row["kind"]      = e.value("kind", "");
        row["ops"]       = e.value("ops", json::array());
        row["lane"]      = e.value("lane", "");
        row["phase"]     = e.value("phase", "");
        row["authority"] = e.value("authority", "candidate_only");
        row["available"] = (e.value("kind", "") == "external_exe") ? !resolve_bin(e).empty() : true;
        arr.push_back(row);
    }
    return json{{"status", "success"}, {"sidecars", arr}};
}

json SidecarStore::describe(const std::string& name) const {
    if (!store_.contains(name))
        return json{{"status", "unknown_sidecar"}, {"name", name}};
    json e = store_[name];
    e["name"] = name;
    if (e.value("kind", "") == "external_exe") {
        std::string bin = resolve_bin(e);
        e["resolved_bin"] = bin;
        e["available"]    = !bin.empty();
    } else {
        e["available"] = true;
    }
    return e;
}

json SidecarStore::call(const std::string& name, const std::string& op, const json& body) const {
    if (!store_.contains(name))
        return json{{"status", "unknown_sidecar"}, {"name", name}};
    const json& e = store_[name];
    const std::string kind = e.value("kind", "");

    if (kind == "xcfe_manifest")
        return json{{"status", "delegated_to_xcfe"}, {"name", name},
                    {"note", "in-process XCFE sidecar; call via SidecarLoader"}};

    if (kind != "external_exe")
        return json{{"status", "unsupported_kind"}, {"name", name}, {"kind", kind}};

    // Optional: enforce the declared op set if present.
    if (e.contains("ops") && e["ops"].is_array() && !e["ops"].empty()) {
        bool ok = false;
        for (const auto& o : e["ops"]) if (o.is_string() && o.get<std::string>() == op) { ok = true; break; }
        if (!ok)
            return json{{"status", "unknown_op"}, {"name", name}, {"op", op}, {"ops", e["ops"]}};
    }

    std::string bin = resolve_bin(e);
    if (bin.empty())
        return json{{"status", "unavailable"}, {"name", name},
                    {"reason", "binary not found"}, {"bin", e.value("bin", "")}};

    return call_external(bin, op, body);
}

// The CLI entry point lives in sw_main.cpp (the standalone `sw` target), so this
// translation unit is a pure library component sharable across the runtimes.
