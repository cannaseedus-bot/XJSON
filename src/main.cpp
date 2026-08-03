#include "runtime.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string manifest_path = "manifest.json";
    int port_override = 0;
    Runtime runtime;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--manifest" || arg == "-m") && i + 1 < argc) {
            manifest_path = argv[++i];
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port_override = std::stoi(argv[++i]);
        } else if ((arg == "--sidecar" || arg == "-s") && i + 1 < argc) {
            runtime.add_sidecar_path(argv[++i]);
        } else if (arg.rfind("--port=", 0) == 0) {
            port_override = std::stoi(arg.substr(7));
        } else if (arg.rfind("--sidecar=", 0) == 0) {
            runtime.add_sidecar_path(arg.substr(10));
        } else if (!arg.empty() && arg[0] != '-') {
            manifest_path = arg;
        }
    }

    runtime.set_port_override(port_override);

    if (!runtime.load_manifest(manifest_path)) {
        std::cerr << "[json_runtime] Failed to load manifest: " << manifest_path << "\n";
        return 1;
    }

    runtime.boot();

    return 0;
}
