// sw_main.cpp -- standalone `sw` CLI over the Sidecar Store (SidecarStore in sw.cpp).
//
//   sw [--manifest <path>] list
//   sw [--manifest <path>] describe <name>
//   sw [--manifest <path>] call <name> <op> [json-body]
//
// The SidecarStore itself lives in sw.cpp, which is also linked into json_runtime /
// sco_runtime so every runtime shares one sidecar dispatch path.
#include "sw.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string manifest = "sidecars.manifest.json";
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--manifest" && i + 1 < argc) { manifest = argv[++i]; }
        else { args.push_back(a); }
    }

    if (args.empty()) {
        std::cerr << "usage: sw [--manifest <path>] list | describe <name> | call <name> <op> [json-body]\n";
        return 2;
    }

    SidecarStore store;
    if (!store.load(manifest)) return 1;

    const std::string& cmd = args[0];
    if (cmd == "list") {
        std::cout << store.list().dump(2) << "\n";
    } else if (cmd == "describe" && args.size() >= 2) {
        std::cout << store.describe(args[1]).dump(2) << "\n";
    } else if (cmd == "call" && args.size() >= 3) {
        json body = json::object();
        if (args.size() >= 4) {
            try { body = json::parse(args[3]); }
            catch (...) { std::cerr << "[sw] invalid json body\n"; return 2; }
        }
        std::cout << store.call(args[1], args[2], body).dump(2) << "\n";
    } else {
        std::cerr << "usage: sw [--manifest <path>] list | describe <name> | call <name> <op> [json-body]\n";
        return 2;
    }
    return 0;
}
