#include "bots.hpp"

bool BotRuntime::is_bot_manifest(const json& descriptor) {
    if (!descriptor.is_object()) return false;
    return descriptor.contains("@bots") || descriptor.contains("bots") ||
           descriptor.contains("@bot") || descriptor.contains("bot") ||
           descriptor.contains("@bot_helpers") || descriptor.contains("bot_helpers") ||
           descriptor.contains("@native_bots") || descriptor.contains("native_bots") ||
           descriptor.contains("@bots_cpp") || descriptor.contains("bots_cpp");
}

json BotRuntime::contract() {
    return json{
        {"@kind", "json_runtime.bot_runtime.v1"},
        {"rule", "Bot manifests are bootstrap JSON. bots.cpp is the native adapter contract, not the source of truth."},
        {"manifest_examples", json::array({"bots.manifest.json", "bot.manifest.json", "micronaut.manifest.json"})},
        {"categories", json::array({"code", "data", "comm", "automation", "learning"})},
        {"pipeline_patterns", json::array({"pipeline", "parallel", "conditional", "ensemble"})},
        {"helper_endpoint", "http://127.0.0.1:5780/run"},
        {"json_shape", json{
            {"pipeline", "string"},
            {"create", "boolean"},
            {"bots", "array"},
            {"input", "object|string"},
            {"payload", "object|string"}
        }}
    };
}

json BotRuntime::build_payload(const json& node, const json& input) {
    json payload = json::object();
    payload["pipeline"] = node.value("@pipeline", node.value("pipeline", "default"));
    payload["create"] = node.value("@create", node.value("create", true));

    if (node.contains("@bots")) payload["bots"] = node["@bots"];
    else if (node.contains("bots")) payload["bots"] = node["bots"];
    else {
        payload["bots"] = json::array({
            json{
                {"name", node.value("@name", node.value("name", "bot"))},
                {"category", node.value("@category", node.value("category", "automation"))},
                {"method", node.value("@method", node.value("method", "run"))},
                {"params", node.value("@params", node.value("params", json::object()))}
            }
        });
    }

    if (node.contains("@input")) payload["input"] = node["@input"];
    else if (node.contains("input")) payload["input"] = node["input"];
    else payload["input"] = input;

    if (node.contains("@payload")) payload["payload"] = node["@payload"];
    else if (node.contains("payload")) payload["payload"] = node["payload"];

    return payload;
}
