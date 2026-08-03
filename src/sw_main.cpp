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

static std::string pad(std::string s, size_t w) {
    if (s.size() < w) s.append(w - s.size(), ' ');
    else if (s.size() > w && w > 1) { s = s.substr(0, w - 1); s.push_back(' '); }
    return s;
}

// Digitize the sidecar store into an AtomicDOM-style terminal FRAME (manifest ->
// terminal blocks; native presentation, no browser CSS).
static void render_atomic_frame(const json& listResult) {
    const json scs = listResult.value("sidecars", json::array());
    int total = (int)scs.size(), avail = 0;
    for (const auto& s : scs) if (s.value("available", false)) avail++;

    std::cout << "+================ SIDECAR STORE ================+\n";
    std::cout << "| route: sidecar://store     backend: terminal  |\n";
    std::cout << "| feed:  sidecars.manifest.json                 |\n";
    std::cout << "+------------------ SIDECARS -------------------+\n";
    for (const auto& s : scs) {
        int nops = (s.contains("ops") && s["ops"].is_array()) ? (int)s["ops"].size() : 0;
        std::cout << "| " << pad(s.value("name", ""), 20)
                  << pad(s.value("kind", ""), 14)
                  << pad(s.value("lane", ""), 9)
                  << (s.value("available", false) ? "[up]" : "[--]")
                  << "  ops:" << nops << "\n";
    }
    std::cout << "+-------------------- STATUS -------------------+\n";
    std::cout << "| total: " << total << "    available: " << avail
              << "    authority: candidate_only\n";
    std::cout << "+----------------------------------------------+\n";
}

int main(int argc, char** argv) {
    std::string manifest = "sidecars.manifest.json";
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--manifest" && i + 1 < argc) { manifest = argv[++i]; }
        else { args.push_back(a); }
    }

    if (args.empty()) {
        std::cerr << "usage: sw [--manifest <path>] list | frame | describe <name> | call <name> <op> [json-body]\n";
        return 2;
    }

    SidecarStore store;
    if (!store.load(manifest)) return 1;

    const std::string& cmd = args[0];
    if (cmd == "list") {
        std::cout << store.list().dump(2) << "\n";
    } else if (cmd == "frame") {
        render_atomic_frame(store.list());
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
        std::cerr << "usage: sw [--manifest <path>] list | frame | describe <name> | call <name> <op> [json-body]\n";
        return 2;
    }
    return 0;
}
