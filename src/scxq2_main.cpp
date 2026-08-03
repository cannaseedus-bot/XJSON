/**
 * scxq2_main.cpp -- SCXQ2 Codec CLI
 *
 * Usage:
 *   scxq2_runtime.exe --compile   <in.json>   [out.scxq2]
 *   scxq2_runtime.exe --decompile <in.scxq2>  [out.json]
 *   scxq2_runtime.exe --roundtrip <in.json>   -- compile then decompile, diff
 *   scxq2_runtime.exe --info      <in.scxq2>  -- header + dict summary
 *   scxq2_runtime.exe --extend    <ops.json> --compile <in.json> [out.scxq2]
 *   scxq2_runtime.exe --list-ops              -- list all built-in ops
 */

#include "scxq2.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

using json = nlohmann::json;

// ── helpers ──────────────────────────────────────────────────────────────────

static json load_json(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + path);
    return json::parse(f);
}

static std::vector<uint8_t> load_binary(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("cannot open: " + path);
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

static void write_binary(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

static void write_json(const std::string& path, const json& j) {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot write: " + path);
    f << j.dump(2) << "\n";
}

static std::string replace_ext(const std::string& path, const std::string& new_ext) {
    size_t dot = path.rfind('.');
    size_t sep = path.find_last_of("/\\");
    if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
        return path.substr(0, dot) + new_ext;
    return path + new_ext;
}

static void print_usage() {
    std::cout <<
        "scxq2_runtime -- SCXQ2 Bytecode Compiler/Decompiler\n\n"
        "  --compile   <in.json>  [out.scxq2]     compile XCFE JSON → binary\n"
        "  --decompile <in.scxq2> [out.json]       decompile binary  → XCFE JSON\n"
        "  --roundtrip <in.json>                   compile+decompile; verify lossless\n"
        "  --info      <in.scxq2>                  show header, dict, instruction count\n"
        "  --extend    <ops.json> --compile <in.json> [out.scxq2]  compile with custom ops\n"
        "  --list-ops                              list all built-in opcodes\n";
}

// ── commands ──────────────────────────────────────────────────────────────────

static int cmd_compile(SCX::SCXQ2Codec& codec,
                        const std::string& in_path,
                        std::string out_path) {
    if (out_path.empty()) out_path = replace_ext(in_path, ".scxq2");
    json prog = load_json(in_path);
    auto bytes = codec.compile(prog);
    write_binary(out_path, bytes);
    std::cout << "[scxq2] compiled  " << in_path << "  →  " << out_path
              << "  (" << bytes.size() << " bytes)\n";
    return 0;
}

static int cmd_decompile(SCX::SCXQ2Codec& codec,
                          const std::string& in_path,
                          std::string out_path) {
    if (out_path.empty()) out_path = replace_ext(in_path, ".json");
    auto bytes = load_binary(in_path);
    json prog = codec.decompile(bytes);
    write_json(out_path, prog);
    std::cout << "[scxq2] decompiled " << in_path << "  →  " << out_path << "\n";
    return 0;
}

static int cmd_roundtrip(SCX::SCXQ2Codec& codec, const std::string& in_path) {
    json original = load_json(in_path);
    auto bytes    = codec.compile(original);
    json restored = codec.decompile(bytes);

    std::string orig_str     = original.dump();
    std::string restored_str = restored.dump();

    std::cout << "[scxq2] input:     " << in_path << "\n";
    std::cout << "[scxq2] compiled:  " << bytes.size() << " bytes\n";

    if (orig_str == restored_str) {
        std::cout << "[scxq2] roundtrip: PASS (lossless)\n";
        return 0;
    }

    std::cerr << "[scxq2] roundtrip: FAIL\n";
    std::cerr << "  original:  " << orig_str.substr(0, 200) << "\n";
    std::cerr << "  restored:  " << restored_str.substr(0, 200) << "\n";
    return 1;
}

static int cmd_info(const std::string& in_path) {
    auto bytes = load_binary(in_path);

    if (bytes.size() < 6) {
        std::cerr << "[scxq2] file too small\n";
        return 1;
    }

    // Check magic
    if (memcmp(bytes.data(), SCX::SCXQ2_MAGIC, 4) != 0) {
        std::cerr << "[scxq2] bad magic bytes\n";
        return 1;
    }
    uint8_t version = bytes[4];
    uint8_t dict_len = bytes[5];

    std::cout << "[scxq2] file:    " << in_path << "  (" << bytes.size() << " bytes)\n";
    std::cout << "[scxq2] version: " << (int)version << "\n";
    std::cout << "[scxq2] dict:    " << (int)dict_len << " entries\n";

    // Decode dict
    SCX::StringDict dict;
    size_t code_start = SCX::StringDict::decode(bytes, 5, dict);

    for (size_t i = 0; i < dict.strings.size(); i++) {
        std::cout << "  [" << i << "] " << dict.strings[i] << "\n";
    }

    // Count instructions
    size_t offset = code_start;
    size_t instr_count = 0;
    while (offset < bytes.size()) {
        SCX::Instruction instr;
        size_t next = SCX::Instruction::decode(bytes, offset, instr);
        if (next <= offset) break;
        offset = next;
        instr_count++;
    }
    std::cout << "[scxq2] instructions: " << instr_count << "\n";
    return 0;
}

static int cmd_list_ops() {
    // Print all static ops by walking all group+subop combinations
    static const char* groups[] = {"MEMORY","STRUCT","CONTROL","MATH","LOGIC","TENSOR","MESH","META"};
    static const uint8_t group_ids[] = {0x00,0x04,0x08,0x0C,0x10,0x14,0x18,0x1C};
    static const int subop_max = 16;

    SCX::SCXQ2Codec codec;
    std::cout << "Built-in SCXQ2 opcodes:\n";
    for (int g = 0; g < 8; g++) {
        std::cout << "\n  [" << groups[g] << "]\n";
        for (int s = 0; s < subop_max; s++) {
            const char* name = codec.name_of(group_ids[g], (uint8_t)s);
            if (name && name[0] != '\0' && strcmp(name, "?") != 0) {
                std::cout << "    " << name
                          << "  (group=0x" << std::hex << (int)group_ids[g]
                          << " subop=" << std::dec << s << ")\n";
            }
        }
    }
    auto custom = codec.custom_ops();
    if (!custom.empty()) {
        std::cout << "\n  [CUSTOM]\n";
        for (const auto& c : custom) std::cout << "    " << c << "\n";
    }
    return 0;
}

// ── entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        print_usage();
        return args.empty() ? 0 : 0;
    }

    SCX::SCXQ2Codec codec;

    // Check for --extend <ops.json> prefix
    size_t i = 0;
    if (args[i] == "--extend" && args.size() > 1) {
        try {
            json ext_ops = load_json(args[1]);
            codec.extend(ext_ops);
            std::cout << "[scxq2] extended with " << args[1] << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[scxq2] extend error: " << e.what() << "\n";
            return 1;
        }
        i += 2;
    }

    if (i >= args.size()) { print_usage(); return 1; }

    try {
        if (args[i] == "--compile" && i + 1 < args.size()) {
            std::string out = (i + 2 < args.size()) ? args[i + 2] : "";
            return cmd_compile(codec, args[i + 1], out);
        }
        if (args[i] == "--decompile" && i + 1 < args.size()) {
            std::string out = (i + 2 < args.size()) ? args[i + 2] : "";
            return cmd_decompile(codec, args[i + 1], out);
        }
        if (args[i] == "--roundtrip" && i + 1 < args.size()) {
            return cmd_roundtrip(codec, args[i + 1]);
        }
        if (args[i] == "--info" && i + 1 < args.size()) {
            return cmd_info(args[i + 1]);
        }
        if (args[i] == "--list-ops") {
            return cmd_list_ops();
        }
    } catch (const std::exception& e) {
        std::cerr << "[scxq2] error: " << e.what() << "\n";
        return 1;
    }

    print_usage();
    return 1;
}
