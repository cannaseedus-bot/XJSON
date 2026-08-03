#include "discovery.hpp"

#define CPPHTTPLIB_NO_EXCEPTIONS 1
#include "httplib.h"

#include <iostream>
#include <algorithm>

std::vector<int> Discovery::default_ports() {
    return { 11434, 6001, 7860, 8000, 3000 };
}

bool Discovery::probe(int port, ServiceDescriptor& out) {
    // Use cpp-httplib for a non-blocking connect with short timeout
    httplib::Client cli("127.0.0.1", port);
    cli.set_connection_timeout(0, 300000);  // 300ms
    cli.set_read_timeout(0, 500000);        // 500ms
    cli.set_write_timeout(0, 300000);

    // Try /.well-known/sco first (SCO-native nodes)
    auto r1 = cli.Get("/.well-known/sco");
    if (r1 && r1->status == 200) {
        try {
            json body = json::parse(r1->body);
            out.port         = port;
            out.raw_response = body;
            out.health_url   = "http://127.0.0.1:" + std::to_string(port) + "/.well-known/sco";

            if (body.contains("@sco")) {
                const json& sco = body["@sco"];
                out.service_name = sco.value("id", "unknown");
                if (sco.contains("capabilities")) {
                    for (auto& c : sco["capabilities"])
                        out.capabilities.push_back(c.get<std::string>());
                }
            }
            out.sco_alias = "discovered_" + std::to_string(port);
            return true;
        } catch (...) {}
    }

    // Try /api/health (json-runtime compatible)
    auto r2 = cli.Get("/api/health");
    if (r2 && r2->status == 200) {
        try {
            json body = json::parse(r2->body);
            out.port         = port;
            out.raw_response = body;
            out.health_url   = "http://127.0.0.1:" + std::to_string(port) + "/api/health";
            out.service_name = body.value("runtime", body.value("service", "unknown"));
            out.sco_alias    = "discovered_" + std::to_string(port);

            // Known service fingerprints
            if (port == 11434) { out.service_name = "ollama"; out.capabilities = {"LLM","LOCAL"}; }
            if (port == 7860)  { out.service_name = "gradio"; out.capabilities = {"UI","INFERENCE"}; }
            if (body.contains("runtime") && body["runtime"] == "json-runtime")
                out.capabilities.push_back("xcfe.run");

            return true;
        } catch (...) {}
    }

    // Try GET / for Ollama-style bare endpoint
    auto r3 = cli.Get("/");
    if (r3 && r3->status == 200) {
        out.port         = port;
        out.raw_response = json{{"body", r3->body.substr(0, 200)}};
        out.health_url   = "http://127.0.0.1:" + std::to_string(port) + "/";
        out.service_name = "unknown_" + std::to_string(port);
        out.sco_alias    = "discovered_" + std::to_string(port);
        return true;
    }

    return false;
}

std::vector<ServiceDescriptor> Discovery::scan(const std::vector<int>& ports) {
    std::vector<ServiceDescriptor> results;
    for (int port : ports) {
        ServiceDescriptor desc;
        std::cout << "[discovery] probing port " << port << "...\n";
        if (probe(port, desc)) {
            std::cout << "[discovery]   found: " << desc.service_name
                      << " caps=[";
            for (size_t i = 0; i < desc.capabilities.size(); i++) {
                if (i) std::cout << ",";
                std::cout << desc.capabilities[i];
            }
            std::cout << "]\n";
            results.push_back(desc);
        }
    }
    return results;
}

json Discovery::to_json(const std::vector<ServiceDescriptor>& services) {
    json arr = json::array();
    for (auto& s : services) {
        json caps = json::array();
        for (auto& c : s.capabilities) caps.push_back(c);
        arr.push_back(json{
            {"port",         s.port},
            {"service",      s.service_name},
            {"capabilities", caps},
            {"health_url",   s.health_url},
            {"sco_alias",    s.sco_alias}
        });
    }
    return arr;
}
