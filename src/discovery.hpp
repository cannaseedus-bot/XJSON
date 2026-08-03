#pragma once
// discovery.hpp -- Auto-discovery of local services via safe port probing
//
// Probes known localhost ports for services that expose /.well-known/sco
// or /api/health endpoints. Results are returned as ServiceDescriptors
// which the runtime registers into the SCORegistry.
//
// User-gated: discovery runs at boot and registers endpoints as
// "discovered://port" SCO aliases. The user must explicitly route to
// them via CALL or bridge config.

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct ServiceDescriptor {
    int                      port;
    std::string              service_name;
    std::vector<std::string> capabilities;
    std::string              health_url;
    std::string              sco_alias;    // "discovered_11434"
    json                     raw_response; // raw /api/health or /.well-known/sco
};

class Discovery {
public:
    // Probe a list of ports. Returns descriptors for live services.
    std::vector<ServiceDescriptor> scan(const std::vector<int>& ports);

    // Default known port list
    static std::vector<int> default_ports();

    // Export discovered services as JSON
    static json to_json(const std::vector<ServiceDescriptor>& services);

private:
    bool probe(int port, ServiceDescriptor& out);
};
