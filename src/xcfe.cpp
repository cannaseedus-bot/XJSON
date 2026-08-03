#include "xcfe.hpp"
#include "sco.hpp"
#include "sidecar.hpp"
#include "bots.hpp"
#include "gpu_dispatch.hpp"
#include "tensor_runtime.hpp"

#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// SHA-256 (no external deps)
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

static inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

std::string XCFE::sha256_hex(const std::string& msg) {
    uint32_t h[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };

    std::string m = msg;
    size_t len = m.size();
    m += '\x80';
    while ((m.size() % 64) != 56) m += '\x00';
    uint64_t bitlen = (uint64_t)len * 8;
    for (int i = 7; i >= 0; i--) m += (char)((bitlen >> (i * 8)) & 0xff);

    for (size_t i = 0; i < m.size(); i += 64) {
        uint32_t w[64];
        for (int j = 0; j < 16; j++) {
            w[j] = ((uint8_t)m[i+j*4]   << 24) | ((uint8_t)m[i+j*4+1] << 16)
                 | ((uint8_t)m[i+j*4+2] <<  8) |  (uint8_t)m[i+j*4+3];
        }
        for (int j = 16; j < 64; j++) {
            uint32_t s0 = rotr32(w[j-15],7) ^ rotr32(w[j-15],18) ^ (w[j-15]>>3);
            uint32_t s1 = rotr32(w[j-2],17) ^ rotr32(w[j-2],19)  ^ (w[j-2]>>10);
            w[j] = w[j-16] + s0 + w[j-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int j = 0; j < 64; j++) {
            uint32_t S1  = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
            uint32_t ch  = (e & f) ^ (~e & g);
            uint32_t t1  = hh + S1 + ch + K256[j] + w[j];
            uint32_t S0  = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2  = S0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    char buf[65];
    for (int i = 0; i < 8; i++)
        snprintf(buf + i*8, 9, "%08x", h[i]);
    return std::string(buf, 64);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tiny infix expression evaluator
// Supports: numbers, $vars, +, -, *, /, %, ** , ==, !=, <, >, <=, >=, &&, ||, !
// ─────────────────────────────────────────────────────────────────────────────

struct ExprParser {
    const std::string& src;
    size_t pos = 0;
    scope_t& scope;
    scope_t& global;

    void skip() { while (pos < src.size() && src[pos] == ' ') ++pos; }

    double parseExpr();
    double parseCmp();
    double parseAddSub();
    double parseMulDiv();
    double parseUnary();
    double parsePrimary();

    double resolveVar(const std::string& name) {
        auto it = scope.find(name);
        if (it != scope.end()) {
            const json& v = it->second;
            if (v.is_number()) return v.get<double>();
            if (v.is_boolean()) return v.get<bool>() ? 1.0 : 0.0;
        }
        auto git = global.find(name);
        if (git != global.end()) {
            const json& v = git->second;
            if (v.is_number()) return v.get<double>();
            if (v.is_boolean()) return v.get<bool>() ? 1.0 : 0.0;
        }
        return 0.0;
    }
};

double ExprParser::parseExpr() {
    double left = parseCmp();
    skip();
    while (pos + 1 < src.size()) {
        if (src[pos] == '&' && src[pos+1] == '&') { pos += 2; left = (left != 0.0 && parseCmp() != 0.0) ? 1.0 : 0.0; }
        else if (src[pos] == '|' && src[pos+1] == '|') { pos += 2; left = (left != 0.0 || parseCmp() != 0.0) ? 1.0 : 0.0; }
        else break;
        skip();
    }
    return left;
}

double ExprParser::parseCmp() {
    double left = parseAddSub();
    skip();
    while (pos < src.size()) {
        if (pos+1 < src.size() && src[pos]=='=' && src[pos+1]=='=') { pos+=2; left = (left == parseAddSub()) ? 1.0 : 0.0; }
        else if (pos+1 < src.size() && src[pos]=='!' && src[pos+1]=='=') { pos+=2; left = (left != parseAddSub()) ? 1.0 : 0.0; }
        else if (pos+1 < src.size() && src[pos]=='<' && src[pos+1]=='=') { pos+=2; left = (left <= parseAddSub()) ? 1.0 : 0.0; }
        else if (pos+1 < src.size() && src[pos]=='>' && src[pos+1]=='=') { pos+=2; left = (left >= parseAddSub()) ? 1.0 : 0.0; }
        else if (src[pos]=='<') { ++pos; left = (left < parseAddSub()) ? 1.0 : 0.0; }
        else if (src[pos]=='>') { ++pos; left = (left > parseAddSub()) ? 1.0 : 0.0; }
        else break;
        skip();
    }
    return left;
}

double ExprParser::parseAddSub() {
    double left = parseMulDiv();
    skip();
    while (pos < src.size()) {
        if (src[pos]=='+') { ++pos; left += parseMulDiv(); }
        else if (src[pos]=='-') { ++pos; left -= parseMulDiv(); }
        else break;
        skip();
    }
    return left;
}

double ExprParser::parseMulDiv() {
    double left = parseUnary();
    skip();
    while (pos < src.size()) {
        if (pos+1 < src.size() && src[pos]=='*' && src[pos+1]=='*') { pos+=2; left = std::pow(left, parseUnary()); }
        else if (src[pos]=='*') { ++pos; left *= parseUnary(); }
        else if (src[pos]=='/') { ++pos; double r = parseUnary(); left = (r!=0) ? left/r : 0.0; }
        else if (src[pos]=='%') { ++pos; double r = parseUnary(); left = std::fmod(left, r); }
        else break;
        skip();
    }
    return left;
}

double ExprParser::parseUnary() {
    skip();
    if (pos < src.size() && src[pos]=='!') { ++pos; return parsePrimary() == 0.0 ? 1.0 : 0.0; }
    if (pos < src.size() && src[pos]=='-') { ++pos; return -parsePrimary(); }
    return parsePrimary();
}

double ExprParser::parsePrimary() {
    skip();
    if (pos < src.size() && src[pos]=='(') {
        ++pos;
        double v = parseExpr();
        if (pos < src.size() && src[pos]==')') ++pos;
        return v;
    }
    if (pos < src.size() && src[pos]=='$') {
        ++pos;
        size_t start = pos;
        while (pos < src.size() && (isalnum(src[pos]) || src[pos]=='_')) ++pos;
        return resolveVar(src.substr(start, pos - start));
    }
    // number
    size_t start = pos;
    while (pos < src.size() && (isdigit(src[pos]) || src[pos]=='.' || (pos==start && src[pos]=='-'))) ++pos;
    if (pos == start) return 0.0;
    return std::stod(src.substr(start, pos - start));
}

json XCFE::eval_expr(const std::string& expr, scope_t& scope) {
    ExprParser p{expr, 0, scope, global_state};
    double v = p.parseExpr();
    // Return bool if result is exactly 0 or 1 (from comparisons)
    if (v == 1.0 || v == 0.0) {
        // Check if any comparison operator is present
        for (const char* op : {"==","!=","<=",">=","<",">","&&","||","!"}) {
            if (expr.find(op) != std::string::npos) return v != 0.0;
        }
    }
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static double numval(const json& v) {
    if (v.is_number())  return v.get<double>();
    if (v.is_boolean()) return v.get<bool>() ? 1.0 : 0.0;
    throw std::runtime_error("xcfe: expected number, got: " + v.dump());
}

static bool truthy(const json& v) {
    if (v.is_null())    return false;
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number())  return v.get<double>() != 0.0;
    if (v.is_string())  return !v.get<std::string>().empty();
    if (v.is_array())   return !v.empty();
    if (v.is_object())  return !v.empty();
    return false;
}

static constexpr double KUHUL_PI = 3.14159265358979323846264338327950288;

static std::string canonical_phase_name(std::string value) {
    std::string key;
    key.reserve(value.size());
    for (char ch : value) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch)) key.push_back(static_cast<char>(std::tolower(uch)));
    }
    if (key == "pop") return "Pop";
    if (key == "wo") return "Wo";
    if (key == "yax") return "Yax";
    if (key == "sek") return "Sek";
    if (key == "chen") return "Ch'en";
    if (key == "xul") return "Xul";
    return value;
}

static bool is_kuhul_phase(const std::string& phase) {
    const std::string p = canonical_phase_name(phase);
    return p == "Pop" || p == "Wo" || p == "Yax" || p == "Sek" || p == "Ch'en" || p == "Xul";
}

static double phase_angle(const std::string& phase) {
    const std::string p = canonical_phase_name(phase);
    if (p == "Pop")   return 0.0;
    if (p == "Wo")    return KUHUL_PI / 3.0;
    if (p == "Yax")   return 2.0 * KUHUL_PI / 3.0;
    if (p == "Sek")   return KUHUL_PI;
    if (p == "Ch'en") return 4.0 * KUHUL_PI / 3.0;
    if (p == "Xul")   return 5.0 * KUHUL_PI / 3.0;
    return 0.0;
}

static std::string phase_glyph_name(const std::string& phase) {
    const std::string p = canonical_phase_name(phase);
    if (p == "Pop")   return "perceive.circle";
    if (p == "Wo")    return "represent.hex";
    if (p == "Yax")   return "plan.triangle";
    if (p == "Sek")   return "execute.star";
    if (p == "Ch'en") return "project.diamond";
    if (p == "Xul")   return "consolidate.hex";
    return "unknown";
}

static std::string next_phase(const std::string& phase) {
    const std::string p = canonical_phase_name(phase);
    if (p == "Pop")   return "Wo";
    if (p == "Wo")    return "Yax";
    if (p == "Yax")   return "Sek";
    if (p == "Sek")   return "Ch'en";
    if (p == "Ch'en") return "Xul";
    return "Pop";
}

static bool legal_phase_transition(const std::string& from, const std::string& to) {
    return next_phase(from) == canonical_phase_name(to);
}

static json phase_state_object(const std::string& phase, const json& history = json::array(),
                               const json& fold = nullptr) {
    const std::string p = canonical_phase_name(phase);
    return json{
        {"current", p},
        {"angle", phase_angle(p)},
        {"angle_pi", phase_angle(p) / KUHUL_PI},
        {"glyph", phase_glyph_name(p)},
        {"history", history.is_array() ? history : json::array()},
        {"fold", fold}
    };
}

static json phase_manifold_object() {
    return json{
        {"@kind", "kuhul.phase.manifold.v1"},
        {"cycle", json::array({"Pop", "Wo", "Yax", "Sek", "Ch'en", "Xul"})},
        {"transitions", json{
            {"Pop", json::array({"Wo"})},
            {"Wo", json::array({"Yax"})},
            {"Yax", json::array({"Sek"})},
            {"Sek", json::array({"Ch'en"})},
            {"Ch'en", json::array({"Xul"})},
            {"Xul", json::array({"Pop"})}
        }},
        {"phases", json::array({
            json{{"name","Pop"},   {"angle_pi",0.0},             {"glyph",phase_glyph_name("Pop")},   {"operation","perceive_load"}},
            json{{"name","Wo"},    {"angle_pi",1.0 / 3.0},       {"glyph",phase_glyph_name("Wo")},    {"operation","represent_build"}},
            json{{"name","Yax"},   {"angle_pi",2.0 / 3.0},       {"glyph",phase_glyph_name("Yax")},   {"operation","plan_schedule"}},
            json{{"name","Sek"},   {"angle_pi",1.0},             {"glyph",phase_glyph_name("Sek")},   {"operation","execute_compute"}},
            json{{"name","Ch'en"}, {"angle_pi",4.0 / 3.0},       {"glyph",phase_glyph_name("Ch'en")}, {"operation","project_output"}},
            json{{"name","Xul"},   {"angle_pi",5.0 / 3.0},       {"glyph",phase_glyph_name("Xul")},   {"operation","consolidate_replay"}}
        })},
        {"instructions", json::array({"PHASE", "FOLD", "TRANSITION"})},
        {"rule", "JSON declares phase intent; XCFE/KUHUL admits legal transitions and records phase state."}
    };
}

static std::string string_field(const json& node, const char* at_key, const char* plain_key,
                                scope_t& scope, XCFE* engine) {
    if (node.contains(at_key)) {
        json value = engine->eval(node[at_key], scope);
        if (value.is_string()) return value.get<std::string>();
    }
    if (node.contains(plain_key)) {
        json value = engine->eval(node[plain_key], scope);
        if (value.is_string()) return value.get<std::string>();
    }
    return "";
}

static json normalized_step(json step) {
    if (step.is_object() && !step.contains("@op") && step.contains("op") && step["op"].is_string()) {
        step["@op"] = step["op"];
    }
    return step;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

XCFE::XCFE() { register_primitives(); }

// ─────────────────────────────────────────────────────────────────────────────
// Op registry
// ─────────────────────────────────────────────────────────────────────────────

void XCFE::load_ops(const json& ops_map) {
    for (auto& [name, def] : ops_map.items())
        op_defs[name] = def;
}

std::vector<std::string> XCFE::list_ops() const {
    std::vector<std::string> v;
    v.reserve(op_defs.size() + primitives.size());
    for (auto& [k, _] : op_defs)    v.push_back(k);
    for (auto& [k, _] : primitives) v.push_back(k);
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Arg resolution
// ─────────────────────────────────────────────────────────────────────────────

json XCFE::resolve_arg(const json& arg, scope_t& scope) {
    if (arg.is_string()) {
        std::string s = arg.get<std::string>();
        if (!s.empty() && s[0] == '$') {
            auto key = s.substr(1);
            auto it = scope.find(key);
            if (it != scope.end()) return it->second;
            auto git = global_state.find(key);
            if (git != global_state.end()) return git->second;
            return nullptr;
        }
        auto it = scope.find(s);
        if (it != scope.end()) return it->second;
    }
    return arg;
}

// ─────────────────────────────────────────────────────────────────────────────
// ── PRIMITIVE REGISTRATION ─────────────────────────────────────────────────
// Five natives: READ, WRITE, EVAL, HASH, CALL
// ─────────────────────────────────────────────────────────────────────────────

void XCFE::register_primitives() {

    // ── native.READ ──────────────────────────────────────────────────────────
    // Reads from scope / global_state.
    // @fn: read | exists | keys | values | size
    primitives["native.READ"] = [this](const json& node, scope_t& scope) -> json {
        std::string fn = node.value("@fn", "read");

        if (fn == "read") {
            std::string key;
            if (node.contains("@key")) key = resolve_arg(node["@key"], scope).get<std::string>();
            auto it = scope.find(key);
            if (it != scope.end()) return it->second;
            auto git = global_state.find(key);
            if (git != global_state.end()) return git->second;
            return nullptr;
        }
        if (fn == "exists") {
            std::string key;
            if (node.contains("@key")) key = resolve_arg(node["@key"], scope).get<std::string>();
            return scope.count(key) > 0 || global_state.count(key) > 0;
        }
        if (fn == "keys") {
            json ks = json::array();
            for (auto& [k, _] : global_state) ks.push_back(k);
            return ks;
        }
        if (fn == "values") {
            json vs = json::array();
            for (auto& [_, v] : global_state) vs.push_back(v);
            return vs;
        }
        if (fn == "size") {
            return static_cast<int>(global_state.size());
        }
        throw std::runtime_error("native.READ: unknown fn: " + fn);
    };

    // ── native.WRITE ─────────────────────────────────────────────────────────
    // Writes to scope AND global_state (persistent).
    // @fn: write | delete
    primitives["native.WRITE"] = [this](const json& node, scope_t& scope) -> json {
        std::string fn = node.value("@fn", "write");

        if (fn == "write") {
            std::string key;
            if (node.contains("@key"))   key = resolve_arg(node["@key"], scope).get<std::string>();
            if (node.contains("@addr"))  key = resolve_arg(node["@addr"], scope).get<std::string>();
            if (key.empty() && node.contains("key"))      key = resolve_arg(node["key"], scope).get<std::string>();
            if (key.empty() && node.contains("addr"))     key = resolve_arg(node["addr"], scope).get<std::string>();
            if (key.empty() && node.contains("@register")) key = resolve_arg(node["@register"], scope).get<std::string>();
            if (key.empty() && node.contains("register")) key = resolve_arg(node["register"], scope).get<std::string>();
            json val = nullptr;
            if (node.contains("@value")) val = resolve_arg(node["@value"], scope);
            else if (node.contains("value")) val = resolve_arg(node["value"], scope);
            else if (global_state.find("__stack") != global_state.end() && global_state["__stack"].is_array() &&
                     !global_state["__stack"].empty()) {
                val = global_state["__stack"].back();
                global_state["__stack"].erase(global_state["__stack"].end() - 1);
                scope["__stack"] = global_state["__stack"];
            }
            if (key.empty()) throw std::runtime_error("native.WRITE: missing key/register");
            global_state[key] = val;
            scope[key] = val;
            return val;
        }
        if (fn == "delete") {
            std::string key;
            if (node.contains("@key")) key = resolve_arg(node["@key"], scope).get<std::string>();
            global_state.erase(key);
            scope.erase(key);
            return true;
        }
        throw std::runtime_error("native.WRITE: unknown fn: " + fn);
    };

    // ── native.STACK ─────────────────────────────────────────────────────────
    // Small runtime stack for JSON programs using PUSH/POP notation.
    primitives["native.STACK"] = [this](const json& node, scope_t& scope) -> json {
        std::string fn = node.value("@fn", "push");
        json stack = global_state.find("__stack") != global_state.end() && global_state["__stack"].is_array()
                         ? global_state["__stack"]
                         : json::array();

        if (fn == "push") {
            json value = nullptr;
            if (node.contains("@value")) value = resolve_arg(node["@value"], scope);
            else if (node.contains("value")) value = resolve_arg(node["value"], scope);
            else if (node.contains("@in") && node["@in"].is_array() && !node["@in"].empty())
                value = resolve_arg(node["@in"][0], scope);
            stack.push_back(value);
            global_state["__stack"] = stack;
            scope["__stack"] = stack;
            return value;
        }

        if (fn == "pop") {
            if (stack.empty()) return nullptr;
            json value = stack.back();
            stack.erase(stack.end() - 1);
            global_state["__stack"] = stack;
            scope["__stack"] = stack;
            return value;
        }

        throw std::runtime_error("native.STACK: unknown fn: " + fn);
    };

    // ── native.PROGRAM ───────────────────────────────────────────────────────
    // Executes a named inline function from a JSON program's functions table.
    primitives["native.PROGRAM"] = [this](const json& node, scope_t& scope) -> json {
        std::string fn = node.value("@fn", "exec");
        if (fn != "exec") throw std::runtime_error("native.PROGRAM: unknown fn: " + fn);

        std::string name = string_field(node, "@program", "program", scope, this);
        if (name.empty()) name = string_field(node, "@name", "name", scope, this);
        if (name.empty()) throw std::runtime_error("native.PROGRAM: missing program");

        json functions = global_state.count("__program_functions")
                             ? global_state["__program_functions"]
                             : json::object();
        if (!functions.is_object() || !functions.contains(name) || !functions[name].is_array()) {
            return json{{"error", "function_not_found"}, {"program", name}};
        }

        json results = json::array();
        for (const auto& raw_step : functions[name]) {
            json step = normalized_step(raw_step);
            results.push_back(eval(step, scope));
        }
        return json{{"ok", true}, {"program", name}, {"results", results}};
    };

    // ── native.PHASE ─────────────────────────────────────────────────────────
    // K'UHUL phase/fold/transition instruction surface.
    primitives["native.PHASE"] = [this](const json& node, scope_t& scope) -> json {
        std::string fn = node.value("@fn", "phase");

        auto current = [&]() -> std::string {
            if (global_state.find("__kuhul_phase") != global_state.end() && global_state["__kuhul_phase"].is_object() &&
                global_state["__kuhul_phase"].contains("current") &&
                global_state["__kuhul_phase"]["current"].is_string()) {
                return canonical_phase_name(global_state["__kuhul_phase"]["current"].get<std::string>());
            }
            return "Pop";
        };

        auto history = [&]() -> json {
            if (global_state.find("__kuhul_phase_history") != global_state.end() &&
                global_state["__kuhul_phase_history"].is_array()) {
                return global_state["__kuhul_phase_history"];
            }
            return json::array();
        };

        auto fold = [&]() -> json {
            if (global_state.find("__kuhul_fold") != global_state.end()) return global_state["__kuhul_fold"];
            return nullptr;
        };

        auto write_phase = [&](const std::string& phase, json h, json f = nullptr) -> json {
            json state = phase_state_object(phase, h, f);
            global_state["__kuhul_phase"] = state;
            global_state["__kuhul_phase_history"] = h;
            scope["__kuhul_phase"] = state;
            scope["__kuhul_phase_history"] = h;
            return state;
        };

        if (fn == "manifold") return phase_manifold_object();

        if (fn == "transition") {
            std::string from = string_field(node, "@from", "from", scope, this);
            std::string to = string_field(node, "@to", "to", scope, this);
            if (from.empty()) from = current();
            if (to.empty()) throw std::runtime_error("TRANSITION: missing to");
            from = canonical_phase_name(from);
            to = canonical_phase_name(to);
            bool legal = legal_phase_transition(from, to);
            json result = json{{"from", from}, {"to", to}, {"legal", legal}};
            if (legal && node.value("@apply", node.value("apply", true))) {
                json h = history();
                h.push_back(from);
                result["phase"] = write_phase(to, h, fold());
            } else if (!legal) {
                result["error"] = "Illegal transition";
            }
            return result;
        }

        if (fn == "fold") {
            std::string name = string_field(node, "@name", "name", scope, this);
            if (name.empty()) name = "anonymous_fold";
            json operations = json::array();
            if (node.contains("@operations")) operations = node["@operations"];
            else if (node.contains("operations")) operations = node["operations"];
            if (!operations.is_array()) operations = json::array();

            json results = json::array();
            for (const auto& raw_step : operations) {
                json step = normalized_step(raw_step);
                results.push_back(eval(step, scope));
            }

            json f = json{
                {"name", name},
                {"phase", current()},
                {"angle", phase_angle(current())},
                {"angle_pi", phase_angle(current()) / KUHUL_PI},
                {"operation_count", operations.size()},
                {"result", results}
            };
            global_state["__kuhul_fold"] = f;
            global_state["fold"] = f;
            scope["__kuhul_fold"] = f;
            scope["fold"] = f;
            write_phase(current(), history(), f);
            return f;
        }

        if (fn == "phase") {
            std::string target = "";
            if (node.value("@advance", node.value("advance", false))) {
                target = next_phase(current());
                json h = history();
                h.push_back(current());
                return write_phase(target, h, fold());
            }

            target = string_field(node, "@set", "set", scope, this);
            if (!target.empty()) {
                target = canonical_phase_name(target);
                if (!is_kuhul_phase(target)) throw std::runtime_error("PHASE: unknown phase: " + target);
                return write_phase(target, history(), fold());
            }

            target = string_field(node, "@to", "to", scope, this);
            if (!target.empty()) {
                target = canonical_phase_name(target);
                if (!legal_phase_transition(current(), target)) {
                    throw std::runtime_error("Illegal transition: " + current() + " -> " + target);
                }
                json h = history();
                h.push_back(current());
                return write_phase(target, h, fold());
            }

            return write_phase(current(), history(), fold());
        }

        throw std::runtime_error("native.PHASE: unknown fn: " + fn);
    };

    // ── native.EVAL ──────────────────────────────────────────────────────────
    // The semantic expander. Handles:
    //   @expr           — infix expression string
    //   @fn: add/sub/mul/div/mod/pow/min/max/abs/floor/ceil/sqrt/clamp
    //   @fn: eq/neq/gt/lt/gte/lte/and/or/not/xor
    //   @fn: band/bor/bxor/shl/shr
    //   @fn: if/loop/map/filter/reduce
    //   @fn: log
    //   @fn: define/undef/list/eval
    //   @fn: dispatch (gpu stub)
    primitives["native.EVAL"] = [this](const json& node, scope_t& scope) -> json {
        std::string fn = node.value("@fn", "");

        // ── @expr — infix expression ──────────────────────────────────────
        if (node.contains("@expr")) {
            std::string expr = node["@expr"].get<std::string>();
            return eval_expr(expr, scope);
        }

        // ── @node — evaluate a sub-node ───────────────────────────────────
        if (node.contains("@node")) {
            return eval(resolve_arg(node["@node"], scope), scope);
        }

        // Helper: get up to 2 numeric args from @args or @in
        auto R = [this](const json& a, scope_t& s) {
            json resolved = resolve_arg(a, s);
            if (resolved.is_object() && resolved.contains("@op")) return eval(resolved, s);
            return resolved;
        };
        auto nums = [&](int idx) -> double {
            if (node.contains("@args") && (int)node["@args"].size() > idx)
                return numval(R(node["@args"][idx], scope));
            if (node.contains("@in") && node["@in"].is_array() && (int)node["@in"].size() > idx)
                return numval(R(node["@in"][idx], scope));
            return 0.0;
        };
        auto jarg = [&](int idx) -> json {
            if (node.contains("@args") && (int)node["@args"].size() > idx)
                return R(node["@args"][idx], scope);
            if (node.contains("@in") && node["@in"].is_array() && (int)node["@in"].size() > idx)
                return R(node["@in"][idx], scope);
            return json(nullptr);
        };

        // ── Math ─────────────────────────────────────────────────────────
        if (fn == "add")   return nums(0) + nums(1);
        if (fn == "sub")   return nums(0) - nums(1);
        if (fn == "mul")   return nums(0) * nums(1);
        if (fn == "div")   { double b = nums(1); return b != 0 ? nums(0)/b : json(nullptr); }
        if (fn == "mod")   return std::fmod(nums(0), nums(1));
        if (fn == "pow")   return std::pow(nums(0), nums(1));
        if (fn == "min")   return std::min(nums(0), nums(1));
        if (fn == "max")   return std::max(nums(0), nums(1));
        if (fn == "abs")   return std::abs(nums(0));
        if (fn == "floor") return std::floor(nums(0));
        if (fn == "ceil")  return std::ceil(nums(0));
        if (fn == "sqrt")  return std::sqrt(nums(0));
        if (fn == "clamp") {
            double v = nums(0), lo = nums(1), hi = nums(2);
            if (node.contains("@args") && node["@args"].size() >= 3)
                hi = numval(R(node["@args"][2], scope));
            return std::max(lo, std::min(hi, v));
        }

        // ── Logic ─────────────────────────────────────────────────────────
        if (fn == "eq")  return jarg(0) == jarg(1);
        if (fn == "neq") return jarg(0) != jarg(1);
        if (fn == "gt")  return nums(0) > nums(1);
        if (fn == "lt")  return nums(0) < nums(1);
        if (fn == "gte") return nums(0) >= nums(1);
        if (fn == "lte") return nums(0) <= nums(1);
        if (fn == "and") return truthy(jarg(0)) && truthy(jarg(1));
        if (fn == "or")  return truthy(jarg(0)) || truthy(jarg(1));
        if (fn == "xor") return truthy(jarg(0)) != truthy(jarg(1));
        if (fn == "not") return !truthy(jarg(0));

        // ── Bitwise ───────────────────────────────────────────────────────
        if (fn == "band") return static_cast<double>((int64_t)nums(0) & (int64_t)nums(1));
        if (fn == "bor")  return static_cast<double>((int64_t)nums(0) | (int64_t)nums(1));
        if (fn == "bxor") return static_cast<double>((int64_t)nums(0) ^ (int64_t)nums(1));
        if (fn == "shl")  return static_cast<double>((int64_t)nums(0) << (int64_t)nums(1));
        if (fn == "shr")  return static_cast<double>((int64_t)nums(0) >> (int64_t)nums(1));

        // ── IO ────────────────────────────────────────────────────────────
        if (fn == "log") {
            const json* arr = nullptr;
            if (node.contains("@args")) arr = &node["@args"];
            else if (node.contains("@in")) arr = &node["@in"];

            auto print_one = [&](const json& a) {
                json v = resolve_arg(a, scope);
                if (v.is_string())     std::cout << v.get<std::string>();
                else if (!v.is_null()) std::cout << v.dump();
                else                   std::cout << "<null>";
                std::cout << "\n";
            };

            if (arr) {
                if (arr->is_array()) for (const auto& a : *arr) print_one(a);
                else print_one(*arr);
            }
            return nullptr;
        }

        // ── Flow: IF ──────────────────────────────────────────────────────
        if (fn == "if") {
            json cond_val = nullptr;
            if (node.contains("@cond")) cond_val = resolve_arg(node["@cond"], scope);
            bool cond = truthy(cond_val);
            if (cond && node.contains("@then") && !node["@then"].is_null())
                return eval(node["@then"], scope);
            if (!cond && node.contains("@else") && !node["@else"].is_null())
                return eval(node["@else"], scope);
            return nullptr;
        }

        // ── Flow: LOOP ────────────────────────────────────────────────────
        if (fn == "loop") {
            json items = node.contains("@over") ? resolve_arg(node["@over"], scope) : json::array();
            std::string as = node.value("@as", "_item");
            if (items.is_array()) {
                for (const auto& item : items) {
                    scope[as] = item;
                    if (node.contains("@body")) eval(node["@body"], scope);
                }
            }
            return nullptr;
        }

        // ── Flow: MAP ─────────────────────────────────────────────────────
        if (fn == "map") {
            json items = node.contains("@over") ? resolve_arg(node["@over"], scope) : json::array();
            std::string as = node.value("@as", "_item");
            json result = json::array();
            if (items.is_array() && node.contains("@fn_body")) {
                scope_t inner = scope;
                for (const auto& item : items) {
                    inner[as] = item;
                    result.push_back(eval(node["@fn_body"], inner));
                }
            }
            return result;
        }

        // ── Flow: FILTER ──────────────────────────────────────────────────
        if (fn == "filter") {
            json items = node.contains("@over") ? resolve_arg(node["@over"], scope) : json::array();
            std::string as = node.value("@as", "_item");
            json result = json::array();
            if (items.is_array() && node.contains("@pred")) {
                scope_t inner = scope;
                for (const auto& item : items) {
                    inner[as] = item;
                    if (truthy(eval(node["@pred"], inner))) result.push_back(item);
                }
            }
            return result;
        }

        // ── Flow: REDUCE ──────────────────────────────────────────────────
        if (fn == "reduce") {
            json items = node.contains("@over") ? resolve_arg(node["@over"], scope) : json::array();
            std::string as  = node.value("@as", "_item");
            std::string acc = node.value("@acc", "_acc");
            json result = node.contains("@init") ? resolve_arg(node["@init"], scope) : json(0.0);
            if (items.is_array() && node.contains("@fn_body")) {
                scope_t inner = scope;
                for (const auto& item : items) {
                    inner[as]  = item;
                    inner[acc] = result;
                    result = eval(node["@fn_body"], inner);
                }
            }
            return result;
        }

        // ── Meta: DEFINE_OP ───────────────────────────────────────────────
        if (fn == "define") {
            std::string name = node.value("@name", "");
            if (name.empty() && node.contains("@args") && !node["@args"].empty())
                name = node["@args"][0].get<std::string>();
            if (name.empty()) throw std::runtime_error("DEFINE_OP: missing @name");
            if (!node.contains("@definition"))
                throw std::runtime_error("DEFINE_OP: missing @definition");
            op_defs[name] = node["@definition"];
            std::cout << "[xcfe] defined op: " << name << "\n";
            return true;
        }
        if (fn == "undef") {
            std::string name = node.value("@name", "");
            op_defs.erase(name);
            return true;
        }
        if (fn == "list") {
            json names = json::array();
            for (auto& [k, _] : op_defs) names.push_back(k);
            return names;
        }
        if (fn == "eval") {
            json expr = node.contains("@expr") ? node["@expr"] : json(nullptr);
            return eval(expr, scope);
        }

        // ── GPU kernel compilation ───────────────────────────────────────
        if (fn == "dispatch") {
            return compile_gpu_kernel(node);
        }

        if (fn == "tensor" || fn == "alloc" || fn == "matmul" ||
            fn == "relu" || fn == "softmax" || fn == "copy" ||
            fn == "tensor_register" || fn == "tensor_get" || fn == "tensor_exists" ||
            fn == "tensor_remove" || fn == "tensor_list") {
            json tensor_node = node;
            for (const char* key : {"@a", "@b", "@in", "@tensor", "@value",
                                    "a", "b", "input", "tensor", "value"}) {
                if (tensor_node.contains(key)) tensor_node[key] = resolve_arg(tensor_node[key], scope);
            }
            return tensor_runtime(tensor_node);
        }

        if (fn == "function_call") {
            const std::string name = node.value("@name", node.value("name", ""));
            if (name.empty()) throw std::runtime_error("FUNCTION_CALL: missing function name");
            const json raw_args = node.contains("@args") ? node["@args"] : node.value("args", json::object());
            if (!raw_args.is_object()) throw std::runtime_error("FUNCTION_CALL: args must be an object");

            auto resolve_values = [&](const json& value, const auto& self) -> json {
                if (value.is_string()) return resolve_arg(value, scope);
                if (value.is_array()) {
                    json result = json::array();
                    for (const auto& item : value) result.push_back(self(item, self));
                    return result;
                }
                if (value.is_object()) {
                    json result = json::object();
                    for (const auto& [key, item] : value.items()) result[key] = self(item, self);
                    return result;
                }
                return value;
            };
            const json args = resolve_values(raw_args, resolve_values);

            json value;
            if (name == "math.add") {
                value = args.at("a").get<double>() + args.at("b").get<double>();
            } else if (name == "math.mul") {
                value = args.at("a").get<double>() * args.at("b").get<double>();
            } else if (name == "string.concat") {
                value = args.at("str1").get<std::string>() + args.at("str2").get<std::string>();
            } else if (name == "tensor.matmul" || name == "tensor.gemm" ||
                       name == "tensor.relu" || name == "tensor.softmax") {
                json tensor_node = args;
                if (name == "tensor.matmul" || name == "tensor.gemm") {
                    tensor_node["@fn"] = "matmul";
                    tensor_node["@a"] = args.at("A");
                    tensor_node["@b"] = args.at("B");
                } else {
                    tensor_node["@fn"] = name == "tensor.relu" ? "relu" : "softmax";
                    tensor_node["@in"] = args.at("input");
                }
                value = tensor_runtime(tensor_node);
            } else {
                throw std::runtime_error("FUNCTION_CALL: unregistered function: " + name);
            }
            return json{
                {"call_id", node.value("@id", node.value("id", ""))},
                {"function_name", name},
                {"success", true},
                {"result", value}
            };
        }

        // ── Op negotiation ────────────────────────────────────────────────
        if (fn == "negotiate") {
            std::string target = node.value("@target", "");
            // Return best registered executor URI, or fall back to local
            auto it = executor_registry.find(target);
            if (it != executor_registry.end()) return it->second;
            return target;  // default: use op name itself
        }

        if (fn == "register_executor") {
            std::string name = node.value("@name", "");
            std::string uri  = node.value("@uri", "");
            if (!name.empty() && !uri.empty())
                executor_registry[name] = json{{"uri", uri}, {"cost", node.value("@cost", json::object())}};
            return true;
        }

        throw std::runtime_error("native.EVAL: unknown fn: " + fn);
    };

    // ── native.HASH ──────────────────────────────────────────────────────────
    // Computes SHA-256 of the serialised value.
    // Returns hex string.
    primitives["native.HASH"] = [this](const json& node, scope_t& scope) -> json {
        json val = nullptr;
        if (node.contains("@value")) val = resolve_arg(node["@value"], scope);
        else if (node.contains("@in") && node["@in"].is_array() && !node["@in"].empty())
            val = resolve_arg(node["@in"][0], scope);
        else if (node.contains("@args") && node["@args"].is_array() && !node["@args"].empty())
            val = resolve_arg(node["@args"][0], scope);

        std::string data = val.is_string() ? val.get<std::string>() : val.dump();
        return sha256_hex(data);
    };

    // ── native.CALL ──────────────────────────────────────────────────────────
    // External dispatch. URI schemes:
    //   sco://alias      → SCORegistry resolve + execute
    //   sidecar://name   → SidecarLoader dispatch
    //   http[s]://...    → HTTP call (stub — cpp-httplib available)
    //   discovered://port → auto-discovered local service
    primitives["native.CALL"] = [this](const json& node, scope_t& scope) -> json {
        std::string target;
        if (node.contains("@target")) target = resolve_arg(node["@target"], scope).get<std::string>();
        else if (node.contains("@uri")) target = resolve_arg(node["@uri"], scope).get<std::string>();

        json args = nullptr;
        if (node.contains("@args"))  args = node["@args"];
        if (node.contains("@input")) args = resolve_arg(node["@input"], scope);

        std::string action;
        if (node.contains("@action")) action = resolve_arg(node["@action"], scope).get<std::string>();

        // sco:// → SCORegistry
        if (target.size() >= 6 && target.substr(0,6) == "sco://" && sco != nullptr) {
            std::string alias = target.substr(6);
            SCOObject obj;
            if (sco->resolve(alias, obj)) {
                // If it's a program, execute it
                if (obj.data.contains("@control")) {
                    XCFE child;
                    child.sco = sco;
                    child.sidecars = sidecars;
                    if (!obj.data.contains("@ops") && !op_defs.empty()) {
                        // pass op defs to child
                        json ops_copy = json::object();
                        for (auto& [k, v] : op_defs) ops_copy[k] = v;
                        child.load_ops(ops_copy);
                    }
                    child.execute(obj.data);
                    json result = json::object();
                    for (auto& [k, v] : child.global_state) result[k] = v;
                    return result;
                }
                // Otherwise return the object's payload
                if (obj.data.contains("payload")) return obj.data["payload"];
                return obj.data;
            }
            return json{{"error", "sco_not_found"}, {"uri", target}};
        }

        // sidecar://name/op → SidecarLoader
        if (target.size() >= 10 && target.substr(0,10) == "sidecar://" && sidecars != nullptr) {
            std::string path = target.substr(10);
            std::string name, op_name;
            auto slash = path.find('/');
            if (slash != std::string::npos) {
                name    = path.substr(0, slash);
                op_name = path.substr(slash + 1);
            } else {
                name = path;
                op_name = action.empty() ? "RUN" : action;
            }
            scope_t call_scope = scope;
            if (args.is_object()) {
                for (auto& [k, v] : args.items()) call_scope[k] = v;
            }
            return sidecars->call(name, op_name, call_scope);
        }

        // http:// or https:// — stub
        if (target.size() >= 7 && (target.substr(0,7) == "http://" || target.substr(0,8) == "https://")) {
            std::cout << "[native.CALL] HTTP stub: " << target << "\n";
            return json{{"note", "http_stub"}, {"target", target}};
        }

        // discovered://port/path
        if (target.size() >= 13 && target.substr(0,13) == "discovered://") {
            std::cout << "[native.CALL] discovered stub: " << target << "\n";
            return json{{"note", "discovered_stub"}, {"target", target}};
        }

        return json{{"error", "unknown_target"}, {"uri", target}};
    };

    // ── native.BOT ───────────────────────────────────────────────────────────
    // Builds a deterministic Bot Builder payload from JSON notation.
    // The actual helper server remains an external localhost service:
    //   POST http://127.0.0.1:5780/run
    primitives["native.BOT"] = [this](const json& node, scope_t& scope) -> json {
        json input = json::object();
        if (node.contains("@input")) input = resolve_arg(node["@input"], scope);
        else if (node.contains("input")) input = resolve_arg(node["input"], scope);
        else if (node.contains("@payload")) input = resolve_arg(node["@payload"], scope);

        json payload = BotRuntime::build_payload(node, input);
        std::string endpoint = node.value("@endpoint", node.value("endpoint", "http://127.0.0.1:5780/run"));

        return json{
            {"@kind", "json_runtime.bot.invocation.v1"},
            {"endpoint", endpoint},
            {"payload", payload},
            {"note", "payload built; HTTP dispatch belongs to the bot helper service or a higher-level route"}
        };
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// eval() — recursive evaluator
// ─────────────────────────────────────────────────────────────────────────────

json XCFE::eval(const json& node, scope_t& scope) {
    if (node.is_null()) return nullptr;

    // $var reference
    if (node.is_string()) {
        std::string s = node.get<std::string>();
        if (!s.empty() && s[0] == '$') {
            auto key = s.substr(1);
            auto it = scope.find(key);
            if (it != scope.end()) return it->second;
            auto git = global_state.find(key);
            if (git != global_state.end()) return git->second;
            return nullptr;
        }
        return node;
    }

    json active = node;
    if (active.is_object() && !active.contains("@op") && active.contains("op") && active["op"].is_string()) {
        active["@op"] = active["op"];
    }

    if (!active.is_object() || !active.contains("@op")) return node;

    std::string op = active["@op"].get<std::string>();

    // 1. Look up JSON-defined op
    auto def_it = op_defs.find(op);
    if (def_it != op_defs.end()) {
        const json& def = def_it->second;
        std::string type = def.value("@type", "pure");

        // Primitive binding: merge def keys into call node, dispatch
        if (type == "primitive") {
            std::string impl = def.value("@impl", "");
            auto prim_it = primitives.find(impl);
            if (prim_it == primitives.end())
                throw std::runtime_error("XCFE: primitive not registered: " + impl);

            // Merge all def keys (except @type, @impl) into the call node
            json merged = active;
            for (auto& [k, v] : def.items()) {
                if (k != "@type" && k != "@impl" && !merged.contains(k))
                    merged[k] = v;
            }

            json result = prim_it->second(merged, scope);
            if (active.contains("@out") && !result.is_null())
                scope[active["@out"].get<std::string>()] = result;
            return result;
        }

        // Composed / pure: bind @in params, eval @exec
        scope_t inner = scope;

        if (def.contains("@in") && active.contains("@in")) {
            const json& param_names = def["@in"];
            const json& call_ins    = active["@in"];
            if (param_names.is_array() && call_ins.is_array()) {
                for (size_t i = 0; i < param_names.size() && i < call_ins.size(); i++)
                    inner[param_names[i].get<std::string>()] = resolve_arg(call_ins[i], scope);
            }
        }

        if (!def.contains("@exec")) return nullptr;
        json result = eval(def["@exec"], inner);

        if (!result.is_null()) {
            std::string out_key;
            if (active.contains("@out")) out_key = active["@out"].get<std::string>();
            else if (def.contains("@out")) out_key = def["@out"].get<std::string>();
            if (!out_key.empty()) scope[out_key] = result;
        }
        return result;
    }

    // 2. Direct primitive dispatch
    auto prim_it = primitives.find(op);
    if (prim_it != primitives.end()) {
        json result = prim_it->second(active, scope);
        if (active.contains("@out") && !result.is_null())
            scope[active["@out"].get<std::string>()] = result;
        return result;
    }

    throw std::runtime_error("XCFE: unknown op: " + op);
}

// ─────────────────────────────────────────────────────────────────────────────
// Delta stream: apply_delta
// ─────────────────────────────────────────────────────────────────────────────

json XCFE::apply_delta(json program, const json& delta_ops) {
    for (const auto& d : delta_ops) {
        std::string type   = d.value("@type", "");
        std::string target = d.value("@target", "");

        if (type == "UPDATE") {
            // target: "@state.key" or "@control"
            if (target.substr(0,7) == "@state.") {
                std::string key = target.substr(7);
                program["@state"][key] = d["@value"];
            } else if (target == "@state" && d.contains("@value")) {
                program["@state"] = d["@value"];
            }
        }
        else if (type == "INSERT" && target == "@control") {
            int idx = d.value("@index", (int)program["@control"].size());
            program["@control"].insert(program["@control"].begin() + idx, d["@value"]);
        }
        else if (type == "APPEND" && target == "@control") {
            program["@control"].push_back(d["@value"]);
        }
        else if (type == "DELETE" && target == "@control") {
            int idx = d.value("@index", 0);
            if (idx < (int)program["@control"].size())
                program["@control"].erase(program["@control"].begin() + idx);
        }
    }
    return program;
}

// ─────────────────────────────────────────────────────────────────────────────
// exec_step
// ─────────────────────────────────────────────────────────────────────────────

void XCFE::exec_step(const json& step, scope_t& scope) {
    json active = normalized_step(step);
    std::string op = active.value("@op", "");
    if (op.empty()) return;
    eval(active, scope);
}

// ─────────────────────────────────────────────────────────────────────────────
// execute()
// ─────────────────────────────────────────────────────────────────────────────

void XCFE::execute(const json& program) {
    // Load any inline op definitions
    if (program.contains("@ops")) load_ops(program["@ops"]);
    if (program.contains("functions") && program["functions"].is_object()) {
        global_state["__program_functions"] = program["functions"];
    }
    if (program.contains("name") && program["name"].is_string()) {
        global_state["__program_name"] = program["name"];
    }

    // Seed scope from global + @state
    scope_t state = global_state;
    if (program.contains("@state")) {
        for (auto& [k, v] : program["@state"].items())
            state[k] = v;
    }

    // Run @control sequence
    if (program.contains("@control")) {
        for (const auto& step : program["@control"])
            exec_step(step, state);
    }
    if (!program.contains("@control") && program.contains("instructions") && program["instructions"].is_array()) {
        for (const auto& step : program["instructions"])
            exec_step(step, state);
    }

    // Write back to global state
    for (auto& [k, v] : state) global_state[k] = v;
}
