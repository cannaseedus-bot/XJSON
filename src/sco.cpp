#include "sco.hpp"
#include "file_system.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// SHA-256 (standalone copy -- sco.cpp doesn't depend on xcfe.cpp)
// ─────────────────────────────────────────────────────────────────────────────

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr32s(uint32_t x, int n) { return (x>>n)|(x<<(32-n)); }

static std::string sha256_str(const std::string& msg) {
    uint32_t h[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    std::string m = msg;
    size_t len = m.size();
    m += '\x80';
    while ((m.size() % 64) != 56) m += '\x00';
    uint64_t bitlen = (uint64_t)len * 8;
    for (int i = 7; i >= 0; i--) m += (char)((bitlen >> (i*8)) & 0xff);

    for (size_t i = 0; i < m.size(); i += 64) {
        uint32_t w[64];
        for (int j = 0; j < 16; j++) {
            w[j] = ((uint8_t)m[i+j*4]<<24)|((uint8_t)m[i+j*4+1]<<16)
                  |((uint8_t)m[i+j*4+2]<<8)|(uint8_t)m[i+j*4+3];
        }
        for (int j = 16; j < 64; j++) {
            uint32_t s0 = rotr32s(w[j-15],7)^rotr32s(w[j-15],18)^(w[j-15]>>3);
            uint32_t s1 = rotr32s(w[j-2],17)^rotr32s(w[j-2],19)^(w[j-2]>>10);
            w[j] = w[j-16]+s0+w[j-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int j = 0; j < 64; j++) {
            uint32_t S1=(rotr32s(e,6)^rotr32s(e,11)^rotr32s(e,25));
            uint32_t ch=(e&f)^(~e&g);
            uint32_t t1=hh+S1+ch+K256[j]+w[j];
            uint32_t S0=(rotr32s(a,2)^rotr32s(a,13)^rotr32s(a,22));
            uint32_t maj=(a&b)^(a&c)^(b&c);
            uint32_t t2=S0+maj;
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;
        h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    char buf[65];
    for (int i = 0; i < 8; i++) snprintf(buf+i*8,9,"%08x",h[i]);
    return std::string(buf,64);
}

// ─────────────────────────────────────────────────────────────────────────────

std::string SCORegistry::hash_json(const json& val) {
    return sha256_str(val.is_string() ? val.get<std::string>() : val.dump());
}

// ─────────────────────────────────────────────────────────────────────────────

bool SCORegistry::load(const std::string& registry_path) {
    try {
        json reg = FileSystem::load_json(registry_path);
        const json& aliases = reg.contains("aliases") ? reg["aliases"] : reg;

        for (auto& [alias, entry] : aliases.items()) {
            SCOObject obj;
            obj.alias = alias;
            if (entry.contains("sha256"))   obj.sha256   = entry["sha256"];
            if (entry.contains("path"))     obj.path     = entry["path"];
            if (entry.contains("type"))     obj.type     = entry["type"];
            if (entry.contains("endpoint")) obj.endpoint = entry["endpoint"];
            objects_[alias] = obj;
        }
        std::cout << "[sco] registry loaded: " << objects_.size() << " aliases\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[sco] registry load failed: " << e.what() << "\n";
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

bool SCORegistry::load_object(SCOObject& obj) {
    if (obj.path.empty()) return false;
    // Sandbox: no absolute paths or traversal
    if (obj.path[0] == '/' || obj.path.find("..") != std::string::npos) return false;
    try {
        obj.data = FileSystem::load_json(obj.path);
        return true;
    } catch (...) {
        return false;
    }
}

bool SCORegistry::verify_hash(const SCOObject& obj) {
    if (obj.sha256.empty()) return true;  // no hash declared = trust
    std::string computed = hash_json(obj.data);
    if (computed != obj.sha256) {
        std::cerr << "[sco] hash mismatch for alias=" << obj.alias
                  << " expected=" << obj.sha256 << " got=" << computed << "\n";
        return false;
    }
    return true;
}

bool SCORegistry::resolve(const std::string& alias, SCOObject& out) {
    // Try canonical alias first, then lowercase
    auto it = objects_.find(alias);
    if (it == objects_.end()) {
        std::string lower = alias;
        for (auto& c : lower) c = (char)tolower(c);
        it = objects_.find(lower);
    }
    if (it == objects_.end()) return false;

    SCOObject obj = it->second;

    // API endpoint: just return metadata, no disk load
    if (obj.type == "api") {
        out = obj;
        out.data = json{{"type","api"},{"endpoint",obj.endpoint}};
        return true;
    }

    // Load from disk if not yet loaded
    if (obj.data.is_null() && !obj.path.empty()) {
        if (!load_object(obj)) return false;
        if (!verify_hash(obj)) return false;
        objects_[alias] = obj;  // cache
    }

    out = obj;
    return true;
}

void SCORegistry::register_object(const std::string& alias, SCOObject obj) {
    obj.alias = alias;
    if (obj.sha256.empty() && !obj.data.is_null())
        obj.sha256 = hash_json(obj.data);
    objects_[alias] = std::move(obj);
}

void SCORegistry::register_api(const std::string& alias, const std::string& endpoint) {
    SCOObject obj;
    obj.alias    = alias;
    obj.type     = "api";
    obj.endpoint = endpoint;
    objects_[alias] = obj;
}

json SCORegistry::to_json() const {
    json result = json::object();
    for (auto& [alias, obj] : objects_) {
        json entry;
        if (!obj.sha256.empty())   entry["sha256"]   = obj.sha256;
        if (!obj.path.empty())     entry["path"]      = obj.path;
        if (!obj.type.empty())     entry["type"]      = obj.type;
        if (!obj.endpoint.empty()) entry["endpoint"]  = obj.endpoint;
        result[alias] = entry;
    }
    return result;
}

std::vector<std::string> SCORegistry::list_aliases() const {
    std::vector<std::string> v;
    v.reserve(objects_.size());
    for (auto& [k, _] : objects_) v.push_back(k);
    return v;
}
