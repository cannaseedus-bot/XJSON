#include "scxq2.hpp"
#include <stdexcept>
#include <cstring>

using json = nlohmann::json;

namespace SCX {

// ── Op name lookup table ──────────────────────────────────────────────────────

struct NameEntry { const char* name; uint8_t group; uint8_t subop; };

static const NameEntry NAME_TABLE[] = {
    // MEMORY
    { "READ",         GRP_MEMORY,  Memory::READ    },
    { "WRITE",        GRP_MEMORY,  Memory::WRITE   },
    { "DELETE",       GRP_MEMORY,  Memory::DEL     },
    { "APPEND",       GRP_MEMORY,  Memory::APPEND  },
    { "MOVE",         GRP_MEMORY,  Memory::MOVE    },
    { "COPY",         GRP_MEMORY,  Memory::COPY    },
    { "EXISTS",       GRP_MEMORY,  Memory::EXISTS  },
    { "SIZE",         GRP_MEMORY,  Memory::SIZE    },
    // STRUCT
    { "MERGE",        GRP_STRUCT,  Struct::MERGE   },
    { "DIFF",         GRP_STRUCT,  Struct::DIFF    },
    { "PATCH",        GRP_STRUCT,  Struct::PATCH   },
    { "KEYS",         GRP_STRUCT,  Struct::KEYS    },
    { "VALUES",       GRP_STRUCT,  Struct::VALUES  },
    { "SLICE",        GRP_STRUCT,  Struct::SLICE   },
    { "INDEX",        GRP_STRUCT,  Struct::INDEX   },
    { "FLATTEN",      GRP_STRUCT,  Struct::FLATTEN },
    { "EXPAND",       GRP_STRUCT,  Struct::EXPAND  },
    // CONTROL
    { "IF",           GRP_CONTROL, Control::IF       },
    { "SWITCH",       GRP_CONTROL, Control::SWITCH   },
    { "LOOP",         GRP_CONTROL, Control::LOOP     },
    { "MAP",          GRP_CONTROL, Control::MAP      },
    { "FILTER",       GRP_CONTROL, Control::FILTER   },
    { "REDUCE",       GRP_CONTROL, Control::REDUCE   },
    { "PARALLEL",     GRP_CONTROL, Control::PARALLEL },
    { "SYNC",         GRP_CONTROL, Control::SYNC     },
    { "WAIT",         GRP_CONTROL, Control::WAIT     },
    { "SIGNAL",       GRP_CONTROL, Control::SIGNAL   },
    // MATH
    { "ADD",          GRP_MATH,    Math::ADD   },
    { "SUB",          GRP_MATH,    Math::SUB   },
    { "MUL",          GRP_MATH,    Math::MUL   },
    { "DIV",          GRP_MATH,    Math::DIV   },
    { "MOD",          GRP_MATH,    Math::MOD   },
    { "POW",          GRP_MATH,    Math::POW   },
    { "MIN",          GRP_MATH,    Math::MIN   },
    { "MAX",          GRP_MATH,    Math::MAX   },
    { "CLAMP",        GRP_MATH,    Math::CLAMP },
    { "ABS",          GRP_MATH,    Math::ABS   },
    { "FLOOR",        GRP_MATH,    Math::FLOOR },
    { "CEIL",         GRP_MATH,    Math::CEIL  },
    { "SQRT",         GRP_MATH,    Math::SQRT  },
    // LOGIC / BIT
    { "EQ",           GRP_LOGIC,   Logic::EQ  },
    { "NEQ",          GRP_LOGIC,   Logic::NEQ },
    { "GT",           GRP_LOGIC,   Logic::GT  },
    { "LT",           GRP_LOGIC,   Logic::LT  },
    { "GTE",          GRP_LOGIC,   Logic::GTE },
    { "LTE",          GRP_LOGIC,   Logic::LTE },
    { "AND",          GRP_LOGIC,   Logic::AND },
    { "OR",           GRP_LOGIC,   Logic::OR  },
    { "NOT",          GRP_LOGIC,   Logic::NOT },
    { "XOR",          GRP_LOGIC,   Logic::XOR },
    { "BIT_AND",      GRP_LOGIC+1, Bit::BAND   },
    { "BIT_OR",       GRP_LOGIC+1, Bit::BOR    },
    { "BIT_XOR",      GRP_LOGIC+1, Bit::BXOR   },
    { "BIT_SHL",      GRP_LOGIC+1, Bit::SHL    },
    { "BIT_SHR",      GRP_LOGIC+1, Bit::SHR    },
    // TENSOR
    { "MATMUL",       GRP_TENSOR,  Tensor::MATMUL      },
    { "DOT",          GRP_TENSOR,  Tensor::DOT         },
    { "TRANSPOSE",    GRP_TENSOR,  Tensor::TRANSPOSE   },
    { "SOFTMAX",      GRP_TENSOR,  Tensor::SOFTMAX     },
    { "NORMALIZE",    GRP_TENSOR,  Tensor::NORMALIZE   },
    { "ATTENTION",    GRP_TENSOR,  Tensor::ATTENTION   },
    { "FLASH_ATTN",   GRP_TENSOR,  Tensor::FLASH_ATTN  },
    { "KV_STORE",     GRP_TENSOR,  Tensor::KV_STORE    },
    { "KV_LOAD",      GRP_TENSOR,  Tensor::KV_LOAD     },
    { "GPU_DISPATCH", GRP_TENSOR,  Tensor::DISPATCH    },
    // MESH
    { "LOAD_SIDECAR", GRP_MESH,    Mesh::LOAD_SIDECAR },
    { "UNLOAD",       GRP_MESH,    Mesh::UNLOAD       },
    { "CALL",         GRP_MESH,    Mesh::CALL         },
    { "SPAWN",        GRP_MESH,    Mesh::SPAWN        },
    { "SEND",         GRP_MESH,    Mesh::SEND         },
    { "RECEIVE",      GRP_MESH,    Mesh::RECV         },
    { "BROADCAST",    GRP_MESH,    Mesh::BROADCAST    },
    { "ROUTE",        GRP_MESH,    Mesh::ROUTE        },
    { "MESH_EXEC",    GRP_MESH,    Mesh::MESH_EXEC    },
    { "NEGOTIATE",    GRP_MESH,    Mesh::NEGOTIATE    },
    // META
    { "DEFINE_OP",    GRP_META,    Meta::DEFINE_OP    },
    { "UPDATE_OP",    GRP_META,    Meta::UPDATE_OP    },
    { "DELETE_OP",    GRP_META,    Meta::DELETE_OP    },
    { "EVAL",         GRP_META,    Meta::EVAL         },
    { "COMPILE",      GRP_META,    Meta::COMPILE      },
    { "LIST_OPS",     GRP_META,    Meta::LIST_OPS     },
    // I/O
    { "PRINT",        GRP_META+1,  0 },
    { "LOG",          GRP_META+1,  0 },
    { nullptr, 0, 0 }
};

bool lookup_op(const std::string& name, OpcodeEntry& out) {
    for (int i = 0; NAME_TABLE[i].name; i++) {
        if (name == NAME_TABLE[i].name) {
            out.group = NAME_TABLE[i].group;
            out.subop = NAME_TABLE[i].subop;
            out.mode  = CPU;
            return true;
        }
    }
    return false;
}

const char* op_name(uint8_t group, uint8_t subop) {
    for (int i = 0; NAME_TABLE[i].name; i++) {
        if (NAME_TABLE[i].group == group && NAME_TABLE[i].subop == subop) {
            return NAME_TABLE[i].name;
        }
    }
    return "UNKNOWN";
}

// ── StringDict ────────────────────────────────────────────────────────────────

uint8_t StringDict::intern(const std::string& s) {
    auto it = index.find(s);
    if (it != index.end()) return it->second;
    uint8_t idx = static_cast<uint8_t>(strings.size());
    strings.push_back(s);
    index[s] = idx;
    return idx;
}

std::vector<uint8_t> StringDict::encode() const {
    std::vector<uint8_t> out;
    // count (2 bytes LE)
    uint16_t cnt = static_cast<uint16_t>(strings.size());
    out.push_back(cnt & 0xFF);
    out.push_back((cnt >> 8) & 0xFF);
    for (const auto& s : strings) {
        uint8_t len = static_cast<uint8_t>(std::min(s.size(), (size_t)255));
        out.push_back(len);
        for (uint8_t i = 0; i < len; i++) out.push_back((uint8_t)s[i]);
    }
    return out;
}

size_t StringDict::decode(const std::vector<uint8_t>& bytes, size_t offset, StringDict& out) {
    if (offset + 2 > bytes.size()) throw std::runtime_error("SCXQ2: dict truncated");
    uint16_t cnt = bytes[offset] | (bytes[offset+1] << 8);
    offset += 2;
    for (uint16_t i = 0; i < cnt; i++) {
        if (offset >= bytes.size()) throw std::runtime_error("SCXQ2: dict entry truncated");
        uint8_t len = bytes[offset++];
        if (offset + len > bytes.size()) throw std::runtime_error("SCXQ2: dict string truncated");
        std::string s(bytes.begin() + offset, bytes.begin() + offset + len);
        out.intern(s);
        offset += len;
    }
    return offset;
}

// ── Arg encoding ──────────────────────────────────────────────────────────────

static std::vector<uint8_t> encode_arg(const Arg& a) {
    std::vector<uint8_t> out;
    uint8_t header = (static_cast<uint8_t>(a.type) << 6);
    switch (a.type) {
        case SMALL_INT:
            out.push_back(header | (static_cast<uint8_t>(a.small_int) & 0x3F));
            break;
        case DICT_REF:
            out.push_back(header | (a.dict_ref & 0x3F));
            break;
        case REGISTER:
            out.push_back(header | (a.reg & 0x3F));
            break;
        case ADDRESS:
            out.push_back(header | (a.addr_mode & 0x03) << 4 | (a.addr_offset >> 4));
            out.push_back(a.addr_offset << 4);
            break;
    }
    return out;
}

static size_t decode_arg(const std::vector<uint8_t>& bytes, size_t offset, Arg& out) {
    if (offset >= bytes.size()) throw std::runtime_error("SCXQ2: arg truncated");
    uint8_t b = bytes[offset++];
    out.type = static_cast<ArgType>(b >> 6);
    uint8_t payload = b & 0x3F;
    switch (out.type) {
        case SMALL_INT: out.small_int = static_cast<int8_t>(payload); break;
        case DICT_REF:  out.dict_ref  = payload; break;
        case REGISTER:  out.reg       = payload; break;
        case ADDRESS:
            out.addr_mode   = (payload >> 4) & 0x03;
            out.addr_offset = payload & 0x0F;
            if (offset < bytes.size()) {
                out.addr_offset = (out.addr_offset << 4) | (bytes[offset++] >> 4);
            }
            break;
    }
    return offset;
}

// ── Instruction encode/decode ─────────────────────────────────────────────────

std::vector<uint8_t> Instruction::encode() const {
    std::vector<uint8_t> out;

    // Byte 0: OPCODE(5) | MODE(2) | ARGC_hi(1)
    uint8_t b0 = ((opcode & 0x1F) << 3)
               | ((static_cast<uint8_t>(mode) & 0x03) << 1)
               | ((argc >> 2) & 0x01);
    out.push_back(b0);

    // Byte 1: ARGC_lo(2) | SUBOP(5) | HAS_OUT(1)
    uint8_t b1 = ((argc & 0x03) << 6)
               | ((subop & 0x1F) << 1)
               | (has_out ? 1 : 0);
    out.push_back(b1);

    if (has_out) {
        out.push_back(out_ref);
    }

    for (const auto& a : args) {
        auto ab = encode_arg(a);
        out.insert(out.end(), ab.begin(), ab.end());
    }

    return out;
}

size_t Instruction::decode(const std::vector<uint8_t>& bytes, size_t offset, Instruction& instr) {
    if (offset + 2 > bytes.size()) throw std::runtime_error("SCXQ2: instruction truncated");

    uint8_t b0 = bytes[offset++];
    uint8_t b1 = bytes[offset++];

    instr.opcode  = (b0 >> 3) & 0x1F;
    instr.mode    = static_cast<Mode>((b0 >> 1) & 0x03);
    instr.argc    = ((b0 & 0x01) << 2) | ((b1 >> 6) & 0x03);
    instr.subop   = (b1 >> 1) & 0x1F;
    instr.has_out = (b1 & 0x01) != 0;

    if (instr.has_out) {
        if (offset >= bytes.size()) throw std::runtime_error("SCXQ2: out_ref truncated");
        instr.out_ref = bytes[offset++];
    }

    instr.args.clear();
    for (uint8_t i = 0; i < instr.argc; i++) {
        Arg a{};
        offset = decode_arg(bytes, offset, a);
        instr.args.push_back(a);
    }

    return offset;
}

// ── XCFE node <-> Instruction ─────────────────────────────────────────────────

Instruction node_to_instr(const json& node, StringDict& dict) {
    Instruction instr{};
    instr.mode    = CPU;
    instr.has_out = false;

    std::string op_name_str = node.value("@op", "");
    OpcodeEntry entry{};
    if (lookup_op(op_name_str, entry)) {
        instr.opcode = entry.group;
        instr.subop  = entry.subop;
        instr.mode   = entry.mode;
    } else {
        // Unknown op: encode name as DICT_REF in META group
        instr.opcode = GRP_META;
        instr.subop  = 0x1F;  // custom
        Arg name_arg{};
        name_arg.type     = DICT_REF;
        name_arg.dict_ref = dict.intern(op_name_str);
        instr.args.push_back(name_arg);
    }

    // @out
    if (node.contains("@out")) {
        std::string out_key = node["@out"].get<std::string>();
        instr.has_out = true;
        instr.out_ref = dict.intern(out_key);
    }

    // @in (array of arg refs)
    if (node.contains("@in")) {
        const json& ins = node["@in"];
        auto encode_one = [&](const json& a) {
            Arg arg{};
            if (a.is_string()) {
                std::string s = a.get<std::string>();
                if (!s.empty() && s[0] == '$') {
                    // scope register ref
                    arg.type     = DICT_REF;
                    arg.dict_ref = dict.intern(s.substr(1));
                } else {
                    arg.type     = DICT_REF;
                    arg.dict_ref = dict.intern(s);
                }
            } else if (a.is_number_integer()) {
                int v = a.get<int>();
                arg.type      = SMALL_INT;
                arg.small_int = static_cast<int8_t>(std::max(-128, std::min(127, v)));
            } else if (a.is_number()) {
                // float: store as SMALL_INT truncated (limitation of 6-bit immediate)
                arg.type      = SMALL_INT;
                arg.small_int = static_cast<int8_t>(a.get<double>());
            } else {
                arg.type     = DICT_REF;
                arg.dict_ref = dict.intern(a.dump());
            }
            instr.args.push_back(arg);
        };

        if (ins.is_array()) {
            for (const auto& a : ins) encode_one(a);
        } else {
            encode_one(ins);
        }
    }

    instr.argc = static_cast<uint8_t>(instr.args.size());
    return instr;
}

json instr_to_node(const Instruction& instr, const StringDict& dict) {
    json node;

    // Reconstruct @op name
    const char* name = op_name(instr.opcode, instr.subop);
    node["@op"] = name;

    // @out
    if (instr.has_out) {
        node["@out"] = dict.at(instr.out_ref);
    }

    // @in args
    if (!instr.args.empty()) {
        json ins = json::array();
        for (const auto& a : instr.args) {
            switch (a.type) {
                case SMALL_INT: ins.push_back(a.small_int); break;
                case DICT_REF:  ins.push_back("$" + dict.at(a.dict_ref)); break;
                case REGISTER:  ins.push_back(std::string("$r") + std::to_string(a.reg)); break;
                case ADDRESS:   ins.push_back(std::string("@addr:") + std::to_string(a.addr_offset)); break;
            }
        }
        node["@in"] = ins;
    }

    return node;
}

// ── Program compile/decompile ─────────────────────────────────────────────────

std::vector<uint8_t> compile(const json& program) {
    StringDict dict;
    std::vector<Instruction> instrs;

    // Collect all control nodes
    if (program.contains("@control")) {
        for (const auto& step : program["@control"]) {
            instrs.push_back(node_to_instr(step, dict));
        }
    }

    // Build binary
    std::vector<uint8_t> out;

    // Header: magic(4) + version(1) + flags(1)
    out.insert(out.end(), SCXQ2_MAGIC, SCXQ2_MAGIC + 4);
    out.push_back(SCXQ2_VERSION);
    out.push_back(0x00);  // flags

    // Dict section
    auto dict_bytes = dict.encode();
    out.insert(out.end(), dict_bytes.begin(), dict_bytes.end());

    // Instruction count (4 bytes LE)
    uint32_t cnt = static_cast<uint32_t>(instrs.size());
    out.push_back(cnt & 0xFF);
    out.push_back((cnt >> 8) & 0xFF);
    out.push_back((cnt >> 16) & 0xFF);
    out.push_back((cnt >> 24) & 0xFF);

    // Instructions
    for (const auto& instr : instrs) {
        auto ib = instr.encode();
        out.insert(out.end(), ib.begin(), ib.end());
    }

    return out;
}

json decompile(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 6) throw std::runtime_error("SCXQ2: file too short");
    if (memcmp(bytes.data(), SCXQ2_MAGIC, 4) != 0) throw std::runtime_error("SCXQ2: bad magic");
    if (bytes[4] != SCXQ2_VERSION) throw std::runtime_error("SCXQ2: wrong version");
    // bytes[5] = flags (reserved)

    size_t offset = 6;

    // Decode dict
    StringDict dict;
    offset = StringDict::decode(bytes, offset, dict);

    // Instruction count
    if (offset + 4 > bytes.size()) throw std::runtime_error("SCXQ2: instr count truncated");
    uint32_t cnt = bytes[offset] | (bytes[offset+1]<<8) | (bytes[offset+2]<<16) | (bytes[offset+3]<<24);
    offset += 4;

    // Decode instructions
    json control = json::array();
    for (uint32_t i = 0; i < cnt; i++) {
        Instruction instr{};
        offset = Instruction::decode(bytes, offset, instr);
        control.push_back(instr_to_node(instr, dict));
    }

    json program;
    program["@program"] = "decompiled";
    program["@control"] = control;
    return program;
}

// ── SCXQ2Codec (extensible instance) ─────────────────────────────────────────

SCXQ2Codec::SCXQ2Codec() {
    // Pre-populate with the static table
    for (int i = 0; NAME_TABLE[i].name; i++) {
        OpcodeEntry e;
        e.group = NAME_TABLE[i].group;
        e.subop = NAME_TABLE[i].subop;
        e.mode  = CPU;
        // don't put in ext maps (static table is authoritative for builtins)
        (void)e;
    }
}

void SCXQ2Codec::register_op(const std::string& name, uint8_t group, uint8_t subop, Mode mode) {
    OpcodeEntry e{ group, subop, mode };
    ext_by_name_[name] = e;
    ext_by_code_[(uint32_t)group * 256 + subop] = name;
}

void SCXQ2Codec::extend(const nlohmann::json& op_table) {
    for (auto& [name, entry] : op_table.items()) {
        if (entry.contains("group") && entry.contains("subop")) {
            uint8_t grp = entry["group"].get<uint8_t>();
            uint8_t sub = entry["subop"].get<uint8_t>();
            register_op(name, grp, sub);
        } else {
            // Auto-assign to META user-defined range
            register_op(name, GRP_META, next_user_subop_);
            if (next_user_subop_ < 0x1F) ++next_user_subop_;
        }
    }
}

bool SCXQ2Codec::lookup(const std::string& name, OpcodeEntry& out) const {
    // Check extension table first
    auto it = ext_by_name_.find(name);
    if (it != ext_by_name_.end()) { out = it->second; return true; }
    // Fall through to static table
    return lookup_op(name, out);
}

const char* SCXQ2Codec::name_of(uint8_t group, uint8_t subop) const {
    // Check extension table first
    auto it = ext_by_code_.find((uint32_t)group * 256 + subop);
    if (it != ext_by_code_.end()) return it->second.c_str();
    return op_name(group, subop);
}

std::vector<std::string> SCXQ2Codec::custom_ops() const {
    std::vector<std::string> v;
    for (auto& [k, _] : ext_by_name_) v.push_back(k);
    return v;
}

// compile/decompile delegate to free functions (which use static table)
// Custom-op round-trip: custom ops encode as META group with user subop,
// decompile looks up the extension table first.

std::vector<uint8_t> SCXQ2Codec::compile(const nlohmann::json& program) const {
    return SCX::compile(program);
}

nlohmann::json SCXQ2Codec::decompile(const std::vector<uint8_t>& bytes) const {
    return SCX::decompile(bytes);
}

} // namespace SCX
