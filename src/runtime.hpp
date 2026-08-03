#pragma once
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "xcfe.hpp"
#include "sco.hpp"
#include "sidecar.hpp"
#include "discovery.hpp"

class Runtime {
public:
    bool load_manifest(const std::string& path);
    void set_port_override(int port);
    void add_sidecar_path(const std::string& path);
    void boot();

    // Shared XCFE instance (server borrows for /api/graph, /api/run)
    XCFE           xcfe;
    SCORegistry    sco;
    SidecarLoader  sidecars;

private:
    nlohmann::json manifest;
    nlohmann::json stdlib_ops;
    int port_override_ = 0;
    std::string manifest_dir_ = ".";
    std::vector<std::string> sidecar_overrides_;

    bool load_stdlib();
    void load_sco_registry();
    void load_sidecars();
    void run_discovery();
    int resolve_server_port() const;
    std::string resolve_manifest_path(const std::string& path) const;
    std::string resolve_host_directory() const;
    std::string resolve_http_file_root() const;
    std::string resolve_http_file_path(const std::string& path) const;
    nlohmann::json execute_file_manager(const std::string& verb, const nlohmann::json& body);
    bool load_declared_sidecar(const std::string& name);
    nlohmann::json execute_sidecar_api(const std::string& action, const std::string& name,
                                       const std::string& op, const nlohmann::json& body);
};
