#include "op_add.hpp"

using json = nlohmann::json;
using scope_t = std::unordered_map<std::string, json>;

void op_add(const json& step, scope_t& state) {
    const json& ins = step["@in"];
    std::string a   = ins[0].get<std::string>();
    std::string b   = ins[1].get<std::string>();
    std::string out = step["@out"].get<std::string>();

    double va = state.at(a).get<double>();
    double vb = state.at(b).get<double>();
    state[out] = va + vb;
}
