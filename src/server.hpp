#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <string>

class Server {
public:
    using RunFn       = std::function<nlohmann::json(const std::string& program_path)>;
    using RunInlineFn = std::function<nlohmann::json(const nlohmann::json& program)>;
    using FileFn      = std::function<nlohmann::json(const std::string& path)>;
    using GraphFn     = std::function<nlohmann::json()>;
    using SCOFn       = std::function<nlohmann::json(const std::string& alias)>;
    using DiscoveryFn = std::function<nlohmann::json()>;
    using FileManagerFn = std::function<nlohmann::json(const std::string& verb, const nlohmann::json& body)>;
    using SidecarFn = std::function<nlohmann::json(const std::string& action, const std::string& name,
                                                   const std::string& op, const nlohmann::json& body)>;

    Server(int port, RunFn run_fn, FileFn file_fn, GraphFn graph_fn,
           SCOFn sco_fn, DiscoveryFn discovery_fn,
           RunInlineFn inline_run_fn = nullptr,
           FileManagerFn file_manager_fn = nullptr,
           SidecarFn sidecar_fn = nullptr);

    void start();  // blocks

private:
    int          port_;
    RunFn        run_fn_;
    RunInlineFn  inline_run_fn_;
    FileFn       file_fn_;
    GraphFn      graph_fn_;
    SCOFn        sco_fn_;
    DiscoveryFn  discovery_fn_;
    FileManagerFn file_manager_fn_;
    SidecarFn sidecar_fn_;
};
