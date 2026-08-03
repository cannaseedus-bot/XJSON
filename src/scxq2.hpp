#pragma once
/**
 * scxq2.hpp -- SCXQ2 opcode table + encoder/decoder
 *
 * Bit layout per spec:
 *   Byte 0: [OPCODE(5)][MODE(2)][ARGC_hi(1)]
 *   Byte 1: [ARGC_lo(2)][SUBOP(5)][HAS_OUT(1)]
 *   [if HAS_OUT: 2 bytes -- dict_ref for @out key]
 *   [ARGC args: each (TYPE(2) + VALUE...)]
 *
 * Opcode groups (top 3 bits of 5-bit opcode):
 *   000 = MEMORY   0x00-0x03
 *   001 = STRUCT   0x04-0x07
 *   010 = CONTROL  0x08-0x0B
 *   011 = MATH     0x0C-0x0F
 *   100 = LOGIC    0x10-0x13
 *   101 = TENSOR   0x14-0x17
 *   110 = MESH     0x18-0x1B
 *   111 = META     0x1C-0x1F
 *
 * Arg type (2 bits):
 *   00 SMALL_INT  -- 1 byte signed value (-128..127)
 *   01 DICT_REF   -- 1 byte index into string dictionary
 *   10 REGISTER   -- 1 byte register/scope slot index
 *   11 ADDRESS    -- 2 bytes: mode(1) + offset(1)
 */

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// ── Opcode groups ─────────────────────────────────────────────────────────────

namespace SCX {

enum OpcodeGroup : uint8_t {
    GRP_MEMORY  = 0x00,
    GRP_STRUCT  = 0x04,
    GRP_CONTROL = 0x08,
    GRP_MATH    = 0x0C,
    GRP_LOGIC   = 0x10,
    GRP_TENSOR  = 0x14,
    GRP_MESH    = 0x18,
    GRP_META    = 0x1C,
};

enum Mode : uint8_t {
    CPU  = 0b00,
    GPU  = 0b01,
    HASH = 0b10,
    META = 0b11,
};

enum ArgType : uint8_t {
    SMALL_INT = 0b00,
    DICT_REF  = 0b01,
    REGISTER  = 0b10,
    ADDRESS   = 0b11,
};

// ── Subop tables ──────────────────────────────────────────────────────────────

namespace Memory {
    enum : uint8_t { READ=0, WRITE=1, DEL=2, APPEND=3, MOVE=4, COPY=5, EXISTS=6, SIZE=7 };
}
namespace Struct {
    enum : uint8_t { MERGE=0, DIFF=1, PATCH=2, KEYS=3, VALUES=4, SLICE=5, INDEX=6, FLATTEN=7, EXPAND=8 };
}
namespace Control {
    enum : uint8_t { IF=0, SWITCH=1, LOOP=2, MAP=3, FILTER=4, REDUCE=5, PARALLEL=6, SYNC=7, WAIT=8, SIGNAL=9 };
}
namespace Math {
    enum : uint8_t { ADD=0, SUB=1, MUL=2, DIV=3, MOD=4, POW=5, MIN=6, MAX=7, CLAMP=8, ABS=9, FLOOR=10, CEIL=11, SQRT=12 };
}
namespace Logic {
    enum : uint8_t { EQ=0, NEQ=1, GT=2, LT=3, GTE=4, LTE=5, AND=6, OR=7, NOT=8, XOR=9 };
}
namespace Bit {
    enum : uint8_t { BAND=0, BOR=1, BXOR=2, SHL=3, SHR=4, PACK=5, UNPACK=6 };
}
namespace Tensor {
    enum : uint8_t { MATMUL=0, DOT=1, TRANSPOSE=2, SOFTMAX=3, NORMALIZE=4, ATTENTION=5, FLASH_ATTN=6, KV_STORE=7, KV_LOAD=8, DISPATCH=9 };
}
namespace Mesh {
    enum : uint8_t { LOAD_SIDECAR=0, UNLOAD=1, CALL=2, SPAWN=3, SEND=4, RECV=5, BROADCAST=6, ROUTE=7, MESH_EXEC=8, NEGOTIATE=9 };
}
namespace Meta {
    enum : uint8_t { DEFINE_OP=0, UPDATE_OP=1, DELETE_OP=2, EVAL=3, COMPILE=4, LIST_OPS=5 };
}

// ── Name lookup tables ────────────────────────────────────────────────────────

struct OpcodeEntry {
    uint8_t group;
    uint8_t subop;
    Mode    mode;
};

// Returns OpcodeEntry for a given op name (e.g. "ADD", "IF", "MATMUL")
bool lookup_op(const std::string& name, OpcodeEntry& out);

// Returns op name for given group + subop
const char* op_name(uint8_t group, uint8_t subop);

// ── Instruction ───────────────────────────────────────────────────────────────

struct Arg {
    ArgType type;
    int8_t  small_int;   // SMALL_INT
    uint8_t dict_ref;    // DICT_REF
    uint8_t reg;         // REGISTER
    uint8_t addr_mode;   // ADDRESS
    uint8_t addr_offset; // ADDRESS
};

struct Instruction {
    uint8_t opcode;      // 5 bits (group | variant)
    Mode    mode;        // 2 bits
    uint8_t argc;        // 3 bits
    uint8_t subop;       // 5 bits
    bool    has_out;
    uint8_t out_ref;     // DICT_REF for @out key
    std::vector<Arg> args;

    // Encode to bytes
    std::vector<uint8_t> encode() const;
    // Decode from byte stream at offset; returns new offset
    static size_t decode(const std::vector<uint8_t>& bytes, size_t offset, Instruction& out);
};

// ── Extensible codec class ────────────────────────────────────────────────────
//
// Use SCXQ2Codec when you need runtime-extensible op registration.
// The free compile()/decompile() functions use the static table only.

class SCXQ2Codec {
public:
    SCXQ2Codec();

    // Register a custom op (group=META, subop from user-defined range 0x10-0x1F)
    void register_op(const std::string& name, uint8_t group, uint8_t subop,
                     Mode mode = CPU);

    // Bulk-register ops from a JSON map: { "OP_NAME": { "group": N, "subop": N } }
    void extend(const nlohmann::json& op_table);

    // Resolve name → entry (instance table first, then static table)
    bool lookup(const std::string& name, OpcodeEntry& out) const;

    // Resolve (group, subop) → name
    const char* name_of(uint8_t group, uint8_t subop) const;

    // Compile / decompile using instance registry
    std::vector<uint8_t> compile(const nlohmann::json& program) const;
    nlohmann::json       decompile(const std::vector<uint8_t>& bytes) const;

    // List all registered custom ops
    std::vector<std::string> custom_ops() const;

private:
    std::unordered_map<std::string, OpcodeEntry> ext_by_name_;
    // reverse map (group*256+subop) → name (for decompile)
    std::unordered_map<uint32_t, std::string> ext_by_code_;

    uint8_t next_user_subop_ = 0x10;  // user-defined range in META group
};

// ── Program codec ─────────────────────────────────────────────────────────────

struct StringDict {
    std::vector<std::string>          strings;
    std::unordered_map<std::string, uint8_t> index;  // max 255 entries

    uint8_t intern(const std::string& s);
    const std::string& at(uint8_t i) const { return strings[i]; }
    std::vector<uint8_t> encode() const;
    static size_t decode(const std::vector<uint8_t>& bytes, size_t offset, StringDict& out);
};

// File magic + header
static constexpr uint8_t SCXQ2_MAGIC[4] = { 'S','C','X','Q' };
static constexpr uint8_t SCXQ2_VERSION  = 0x02;

// Compile XCFE JSON program → binary
std::vector<uint8_t> compile(const nlohmann::json& program);

// Decompile binary → XCFE JSON program (lossless round-trip)
nlohmann::json decompile(const std::vector<uint8_t>& bytes);

// Convert one XCFE node → Instruction (uses dict to intern strings)
Instruction node_to_instr(const nlohmann::json& node, StringDict& dict);

// Convert Instruction → XCFE node
nlohmann::json instr_to_node(const Instruction& instr, const StringDict& dict);

} // namespace SCX
