#include "runtime_authority.hpp"
#include "bots.hpp"

#include <initializer_list>

using json = nlohmann::json;

static bool has_any(const json& value, std::initializer_list<const char*> keys) {
    if (!value.is_object()) return false;
    for (const char* key : keys) {
        if (value.contains(key)) return true;
    }
    return false;
}

json RuntimeAuthority::phase_contract() {
    return json{
        {"pop", "load sidecar bootstrap, user/project scope, and requested schema"},
        {"wo", "bind schema symbols, app routes, commands, and op names"},
        {"yax", "build XCFE node graph and admission guards"},
        {"sek", "execute the selected sidecar op through XCFE"},
        {"chen", "emit artifacts, panel events, object state, or route responses"},
        {"xul", "record provenance, score result, and close or re-enter the chart"},
        {"gravity_wells", json{
            {"rule", "Fold gravity wells score context before action selection; the selected well does not bypass XCFE admission."},
            {"score", "mass * affinity - repulsion - saturation_penalty"},
            {"tie_break", json::array({"pop","wo","yax","sek","chen","xul"})},
            {"wells", json{
                {"pop", json{{"mass",0.85},{"attracts",json::array({"input","manifest","sidecar","file","sco","session"})},{"repels",json::array({"write","model_infer","process_spawn"})}}},
                {"wo", json{{"mass",0.90},{"attracts",json::array({"symbols","routes","schemas","tools","models","threads"})},{"repels",json::array({"unbound_execute"})}}},
                {"yax", json{{"mass",0.95},{"attracts",json::array({"schedule","rank","guard","batch","dependencies"})},{"repels",json::array({"unbounded_loop"})}}},
                {"sek", json{{"mass",1.00},{"attracts",json::array({"compute","sidecar_call","model_infer","compile","generate","gpu_dispatch"})},{"repels",json::array({"missing_policy"})}}},
                {"chen", json{{"mass",0.88},{"attracts",json::array({"response","artifact","ui_emit","vector_screen","event"})},{"repels",json::array({"raw_unverified_state"})}}},
                {"xul", json{{"mass",0.92},{"attracts",json::array({"hash","replay","cache","memory","proof","close"})},{"repels",json::array({"dangling_thread","unreduced_batch"})}}}
            }}
        }}
    };
}

json RuntimeAuthority::sidecar_examples() {
    return json::array({
        "server.manifest.json", "server2.manifest.json", "rpc.manifest.json",
        "mcp.manifest.json", "gpu.manifest.json", "dotnet.manifest.json",
        "roslyn.manifest.json", "windows-desktop.manifest.json",
        "windows-script-host.manifest.json", "scripts.manifest.json",
        "cmake.manifest.json", "visual-studio-tools.manifest.json",
        "native-build.manifest.json", "user.manifest.json", "session.manifest.json",
        "database.manifest.json", "projects.manifest.json", "actions.manifest.json",
        "experts.manifest.json", "evolution.manifest.json",
        "commands.manifest.json", "tools.manifest.json", "coder.manifest.json",
        "mx2lm-compiler.manifest.json", "mx2lm-runtime.manifest.json",
        "micronaut-brains.manifest.json", "chat-hive.manifest.json",
        "primeos-agent-spawner.manifest.json", "asx-nnc.manifest.json",
        "gravity-semantic-wells.manifest.json", "kuhul-xsd-grammar.manifest.json",
        "nnc-xsd-neural-grammar.manifest.json", "supernaut-contracts.manifest.json", "bots.manifest.json",
        "bot.manifest.json", "micronaut.manifest.json", "bson.manifest.json",
        "agents.manifest.json", "batches.manifest.json", "threads.manifest.json",
        "processes.manifest.json", "skills.manifest.json", "logs.manifest.json",
        "events.manifest.json", "parse.manifest.json", "validate.manifest.json",
        "compile.manifest.json", "generate.manifest.json", "glyphs.manifest.json",
        "folds.manifest.json", "nodes.manifest.json", "pop.manifest.json",
        "wo.manifest.json", "yax.manifest.json", "sek.manifest.json",
        "chen.manifest.json", "xul.manifest.json", "programs.manifest.json",
        "scxq2.manifest.json", "sco.manifest.json", "game.manifest.json",
        "website.manifest.json", "xjson.manifest.json", "xcfe.manifest.json",
        "xjson-kuhul-autonomous-ai-runtime.manifest.json", "grammar.manifest.json",
        "classes.manifest.json", "ir.manifest.json"
    });
}

json RuntimeAuthority::app_contract(const json& manifest) {
    json contract = json::object();
    if (manifest.contains("@source_manifest") && manifest["@source_manifest"].is_object()) {
        const json& source = manifest["@source_manifest"];
        if (source.contains("_app_contract") && source["_app_contract"].is_object()) {
            contract = source["_app_contract"];
        } else if (source.contains("app_contract") && source["app_contract"].is_object()) {
            contract = source["app_contract"];
        }
    }
    if (contract.empty() && manifest.contains("_app_contract") && manifest["_app_contract"].is_object()) {
        contract = manifest["_app_contract"];
    }
    if (contract.empty() && manifest.contains("@sidecars") && manifest["@sidecars"].is_object() &&
        manifest["@sidecars"].contains("_app_contract") && manifest["@sidecars"]["_app_contract"].is_object()) {
        contract = manifest["@sidecars"]["_app_contract"];
    }
    return contract;
}

std::string RuntimeAuthority::classify_sidecar(const json& cap) {
    std::string role = "sidecar";

    if (has_any(cap, {"@routes", "routes", "@ports", "ports", "@entry", "entry", "@assets", "assets"}))
        role = "app_server_manifest";
    if (has_any(cap, {"@rpc", "rpc", "@jsonrpc", "jsonrpc", "@methods", "methods"}))
        role = "rpc_manifest";
    if (has_any(cap, {"@mcp", "mcp", "@resources", "resources", "@prompts", "prompts"}))
        role = "mcp_manifest";
    if (has_any(cap, {"@gpu", "gpu", "@d3d9", "d3d9", "@d3d10", "d3d10",
                      "@d3d11", "d3d11", "@d3d11_1", "d3d11_1", "@d3d12", "d3d12",
                      "@d3dconfig", "d3dconfig", "@direct3d", "direct3d",
                      "@direct2d", "direct2d", "@d2d1", "d2d1",
                      "@direct3d_core", "direct3d_core", "@direct3d_debug_layer", "direct3d_debug_layer",
                      "@warp", "warp", "@d3d11on12", "d3d11on12",
                      "@app_overrides", "app_overrides", "@d3dcompiler", "d3dcompiler",
                      "@d3dx", "d3dx", "@dxc", "dxc", "@dxcompiler", "dxcompiler",
                      "@dxil", "dxil", "@shader_model_6", "shader_model_6",
                      "@shader_compilers", "shader_compilers", "@graphics_dlls", "graphics_dlls",
                      "@webgpu", "webgpu", "@webgl2", "webgl2", "@threejs", "threejs",
                      "@three_js", "three_js", "@orbit_controls", "orbit_controls",
                      "@web_graphics_assets", "web_graphics_assets", "@opencl", "opencl",
                      "@compute", "compute", "@kernels", "kernels"}))
        role = "gpu_manifest";
    if (has_any(cap, {"@dotnet", "dotnet", "@dotnet_cli", "dotnet_cli",
                      "@sdk_manifests", "sdk_manifests", "@workloads", "workloads",
                      "@workload_manifests", "workload_manifests",
                      "@workload_families", "workload_families"}))
        role = "dotnet_manifest";
    if (has_any(cap, {"@roslyn", "roslyn", "@csc", "csc", "@vbc", "vbc",
                      "@vbcscompiler", "vbcscompiler", "@dotnet_format", "dotnet_format",
                      "@msbuild", "msbuild", "@analyzers", "analyzers",
                      "@source_generators", "source_generators"}))
        role = "roslyn_manifest";
    if (has_any(cap, {"@windows_desktop", "windows_desktop", "@shared_frameworks", "shared_frameworks",
                      "@wpf", "wpf", "@winforms", "winforms", "@webview2", "webview2",
                      "@wwahost", "wwahost", "@uwp", "uwp",
                      "@windows_web_app_host", "windows_web_app_host", "@apphost", "apphost"}))
        role = "windows_desktop_manifest";
    if (has_any(cap, {"@cmd", "cmd", "@command_prompt", "command_prompt",
                      "@batch_files", "batch_files", "@py_launcher", "py_launcher",
                      "@python", "python", "@python_scripts", "python_scripts",
                      "@cscript", "cscript", "@wscript", "wscript",
                      "@windows_script_host", "windows_script_host", "@vbscript", "vbscript",
                      "@jscript", "jscript", "@script_files", "script_files"}))
        role = "windows_script_host_manifest";
    if (has_any(cap, {"@cmake", "cmake", "@ctest", "ctest", "@cpack", "cpack",
                      "@cmake_gui", "cmake_gui", "@cmcldeps", "cmcldeps",
                      "@visual_studio_tools", "visual_studio_tools", "@vcvars", "vcvars",
                      "@msvc", "msvc", "@native_tools", "native_tools",
                      "@cross_tools", "cross_tools", "@toolchain", "toolchain",
                      "@build_presets", "build_presets", "@test_presets", "test_presets",
                      "@package_presets", "package_presets"}))
        role = "native_build_manifest";
    if (has_any(cap, {"@actions", "actions", "@commands", "commands", "@tools", "tools",
                      "@action_classes", "action_classes"}))
        role = "action_command_tool_manifest";
    if (has_any(cap, {"@experts", "experts", "@expert_pool", "expert_pool"}))
        role = "expert_pool_manifest";
    if (has_any(cap, {"@evolution", "evolution", "@mutation", "mutation", "@reward", "reward",
                      "@genome_contract", "genome_contract", "@opcode_surface", "opcode_surface"}))
        role = "evolution_manifest";
    if (has_any(cap, {"@code_generation", "code_generation"}))
        role = "coder_manifest";
    if (has_any(cap, {"@mx2lm", "mx2lm"}))
        role = "mx2lm_compiler_micronaut";
    if (has_any(cap, {"@mx2lm_runtime", "mx2lm_runtime", "@llm", "llm",
                      "@field_system", "field_system", "@ngrams", "ngrams",
                      "@brain_js", "brain_js"}))
        role = "mx2lm_runtime_manifest";
    if (has_any(cap, {"@micronaut_brains", "micronaut_brains", "@profiles", "profiles",
                      "@intents", "intents", "@ngram_routing", "ngram_routing"}))
        role = "micronaut_brain_manifest";
    if (has_any(cap, {"@chat_hive", "chat_hive", "@kernel_processes", "kernel_processes",
                      "@chat_config", "chat_config", "@kuhul_chat_engine", "kuhul_chat_engine"}))
        role = "chat_hive_manifest";
    if (has_any(cap, {"@primeos_agent_spawner", "primeos_agent_spawner",
                      "@micronaut_factory", "micronaut_factory",
                      "@agent_specializations", "agent_specializations",
                      "@static_tunnel_browser", "static_tunnel_browser",
                      "@training_system", "training_system"}))
        role = "primeos_agent_spawner_manifest";
    if (has_any(cap, {"@nnc", "nnc", "@xjson_runtime", "xjson_runtime",
                      "@kuhul_engine", "kuhul_engine", "@scx_compressor", "scx_compressor",
                      "@klh_backend", "klh_backend", "@asx_runtime", "asx_runtime"}))
        role = "asx_nnc_manifest";
    if (has_any(cap, {"@gravity_semantic_wells", "gravity_semantic_wells",
                      "@wells", "wells", "@bigrams", "bigrams",
                      "@semantic_codec", "semantic_codec"}))
        role = "gravity_semantic_wells_manifest";
    if (has_any(cap, {"@kuhul_xsd_grammar", "kuhul_xsd_grammar",
                      "@hierarchy", "hierarchy", "@phase_machine", "phase_machine",
                      "@knumatics", "knumatics", "@xsd_schema", "xsd_schema"}))
        role = "kuhul_xsd_grammar_manifest";
    if (has_any(cap, {"@nnc_xsd_neural_grammar", "nnc_xsd_neural_grammar",
                      "@schema_refs", "schema_refs", "@neural_grammar", "neural_grammar"}))
        role = "nnc_xsd_neural_grammar_manifest";
    if (has_any(cap, {"@supernaut", "supernaut", "@contracts", "contracts", "@runstate", "runstate"}))
        role = "supernaut_contract_manifest";
    if (BotRuntime::is_bot_manifest(cap))
        role = "bot_manifest";
    if (has_any(cap, {"@models", "models", "@model", "model", "@gguf", "gguf",
                      "@mgguf", "mgguf", "@model_server", "model_server",
                      "@adapters", "adapters", "@providers", "providers"}))
        role = "model_manifest";
    if (has_any(cap, {"@bson", "bson", "@documents", "documents", "@collections", "collections",
                      "@binary", "binary", "@codec", "codec"}))
        role = "bson_manifest";
    if (has_any(cap, {"@agents", "agents", "@agent", "agent"}))
        role = "agent_manifest";
    if (has_any(cap, {"@user", "user", "@users", "users", "@identity", "identity",
                      "@memory", "memory", "@permissions", "permissions"}))
        role = "user_manifest";
    if (has_any(cap, {"@session", "session", "@sessions", "sessions",
                      "@conversation", "conversation", "@history", "history"}))
        role = "session_manifest";
    if (has_any(cap, {"@database", "database", "@databases", "databases",
                      "@tables", "tables", "@indexes", "indexes", "@queries", "queries"}))
        role = "database_manifest";
    if (has_any(cap, {"@projects", "projects", "@project", "project",
                      "@workspace", "workspace", "@workspaces", "workspaces"}))
        role = "project_manifest";
    if (has_any(cap, {"@batches", "batches", "@batch", "batch"}))
        role = "batch_manifest";
    if (has_any(cap, {"@threads", "threads", "@thread", "thread"}))
        role = "thread_manifest";
    if (has_any(cap, {"@processes", "processes", "@process", "process"}))
        role = "process_manifest";
    if (has_any(cap, {"@skills", "skills", "@skill", "skill"}))
        role = "skill_manifest";
    if (has_any(cap, {"@logs", "logs", "@log", "log", "@trace", "trace", "@audit", "audit"}))
        role = "log_manifest";
    if (has_any(cap, {"@events", "events", "@event", "event"}))
        role = "event_manifest";
    if (has_any(cap, {"@parse", "parse", "@parser", "parser", "@ast", "ast", "@kast", "kast"}))
        role = "parse_manifest";
    if (has_any(cap, {"@validate", "validate", "@validator", "validator", "@rules", "rules"}))
        role = "validate_manifest";
    if (has_any(cap, {"@compile", "compile", "@compiler", "compiler", "@targets", "targets"}))
        role = "compile_manifest";
    if (has_any(cap, {"@generate", "generate", "@generator", "generator", "@templates", "templates"}))
        role = "generate_manifest";
    if (has_any(cap, {"@micronaut", "micronaut", "@workers", "workers",
                      "@worker", "worker", "@factory", "factory", "@beans", "beans",
                      "@schedules", "schedules", "@events", "events", "@di", "di"}))
        role = "micronaut_manifest";
    if (has_any(cap, {"@glyphs", "glyphs", "@canonical_glyph_registry", "canonical_glyph_registry",
                      "@fold_op_notation", "fold_op_notation", "@symbols", "symbols",
                      "@lanes", "lanes", "@lane_contracts", "lane_contracts",
                      "@opcodes", "opcodes"}))
        role = "glyph_opcode_manifest";
    if (has_any(cap, {"@angles", "angles", "@angle_contracts", "angle_contracts"}))
        role = "angle_manifest";
    if (has_any(cap, {"@lanes", "lanes", "@lane_contracts", "lane_contracts"}))
        role = "lane_manifest";
    if (has_any(cap, {"@phases", "phases", "@folds", "folds", "@fold_algebra", "fold_algebra",
                      "@xcfe", "xcfe", "@nodes", "nodes",
                      "@schema", "schema"}))
        role = "semantic_graph_manifest";
    if (has_any(cap, {"@angles", "angles", "@angle_contracts", "angle_contracts"}))
        role = "angle_manifest";
    if (has_any(cap, {"@lanes", "lanes", "@lane_contracts", "lane_contracts"}))
        role = "lane_manifest";
    if (has_any(cap, {"@program", "program", "@programs", "programs"}))
        role = "program_manifest";
    if (has_any(cap, {"@scxq2", "scxq2", "@sco", "sco"}))
        role = "binary_runtime_manifest";
    if (has_any(cap, {"@phase", "phase", "@phase_id", "phase_id"}))
        role = "phase_manifest";
    if (has_any(cap, {"@xjson", "xjson", "@surface_syntax", "surface_syntax",
                      "@lowering", "lowering", "@canonical_ast", "canonical_ast",
                      "@canonical_json", "canonical_json", "@expr_blocks", "expr_blocks",
                      "@declarative_blocks", "declarative_blocks", "@flow_operator", "flow_operator",
                      "@test_vectors", "test_vectors"}))
        role = "xjson_language_manifest";
    if (has_any(cap, {"@xcfe", "xcfe", "@static_verifier", "static_verifier",
                      "@runtime_walk", "runtime_walk", "@stdlib", "stdlib",
                      "@pack_manifest", "pack_manifest", "@proof_envelope", "proof_envelope",
                      "@signature", "signature", "@policy", "policy",
                      "@crypto_pack", "crypto_pack", "@session_binding", "session_binding",
                      "@scx_chain", "scx_chain", "@capabilities", "capabilities",
                      "@determinism", "determinism"}))
        role = "xcfe_authority_manifest";
    if (has_any(cap, {"@grammar", "grammar", "@grammars", "grammars"}))
        role = "grammar_manifest";
    if (has_any(cap, {"@classes", "classes", "@class", "class"}))
        role = "class_contract_manifest";
    if (has_any(cap, {"@ir", "ir", "@kimd", "kimd", "@linalg", "linalg",
                      "@core_types", "core_types", "@scalar", "scalar",
                      "@vector", "vector", "@matrix", "matrix", "@tensor", "tensor",
                      "@geometry", "geometry", "@surface", "surface", "@cluster", "cluster",
                      "@ir_grammar", "ir_grammar", "@replay_log", "replay_log",
                      "@svg_tessellation", "svg_tessellation", "@render_kernel", "render_kernel",
                      "@d3d11_submission", "d3d11_submission",
                      "@tiny_character_loop", "tiny_character_loop",
                      "@animated_characters", "animated_characters",
                      "@cartoon_character", "cartoon_character",
                      "@character_rig", "character_rig",
                      "@character_rigging", "character_rigging",
                      "@animation_loop", "animation_loop", "@motion", "motion",
                      "@procedural_animation", "procedural_animation",
                      "@character_state_machine", "character_state_machine",
                      "@screen_vector_graphics", "screen_vector_graphics",
                      "@vector_screen", "vector_screen",
                      "@vector_surface", "vector_surface", "@visual_substrate", "visual_substrate",
                      "@svg_nodes", "svg_nodes", "@tensor_surfaces", "tensor_surfaces",
                      "@glyph_ops", "glyph_ops", "@json_columns", "json_columns",
                      "@tensor_io", "tensor_io",
                      "@native_projection_roots", "native_projection_roots"}))
        role = "unified_ir_vector_surface_manifest";

    if (has_any(cap, {"@mx2lm_runtime", "mx2lm_runtime", "@field_system", "field_system",
                      "@ngrams", "ngrams", "@brain_js", "brain_js"}))
        role = "mx2lm_runtime_manifest";
    if (has_any(cap, {"@micronaut_brains", "micronaut_brains", "@profiles", "profiles",
                      "@intents", "intents", "@ngram_routing", "ngram_routing"}))
        role = "micronaut_brain_manifest";
    if (has_any(cap, {"@chat_hive", "chat_hive", "@kernel_processes", "kernel_processes",
                      "@chat_config", "chat_config", "@kuhul_chat_engine", "kuhul_chat_engine"}))
        role = "chat_hive_manifest";
    if (has_any(cap, {"@primeos_agent_spawner", "primeos_agent_spawner",
                      "@micronaut_factory", "micronaut_factory",
                      "@agent_specializations", "agent_specializations",
                      "@static_tunnel_browser", "static_tunnel_browser",
                      "@training_system", "training_system"}))
        role = "primeos_agent_spawner_manifest";
    if (has_any(cap, {"@nnc", "nnc", "@xjson_runtime", "xjson_runtime",
                      "@kuhul_engine", "kuhul_engine", "@scx_compressor", "scx_compressor",
                      "@klh_backend", "klh_backend", "@asx_runtime", "asx_runtime"}))
        role = "asx_nnc_manifest";
    if (has_any(cap, {"@gravity_semantic_wells", "gravity_semantic_wells",
                      "@wells", "wells", "@bigrams", "bigrams",
                      "@semantic_codec", "semantic_codec"}))
        role = "gravity_semantic_wells_manifest";
    if (has_any(cap, {"@kuhul_xsd_grammar", "kuhul_xsd_grammar",
                      "@hierarchy", "hierarchy", "@phase_machine", "phase_machine",
                      "@knumatics", "knumatics", "@xsd_schema", "xsd_schema"}))
        role = "kuhul_xsd_grammar_manifest";
    if (has_any(cap, {"@nnc_xsd_neural_grammar", "nnc_xsd_neural_grammar",
                      "@schema_refs", "schema_refs", "@neural_grammar", "neural_grammar"}))
        role = "nnc_xsd_neural_grammar_manifest";
    if (has_any(cap, {"@crown_matrix", "crown_matrix"}))
        role = "crown_matrix_manifest";
    if (has_any(cap, {"@actor_matrix", "actor_matrix", "@actors", "actors", "@actor_selection", "actor_selection"}))
        role = "actor_matrix_manifest";
    if (has_any(cap, {"@personality_matrix", "personality_matrix", "@matrix_ops", "matrix_ops"}))
        role = "personality_matrix_manifest";
    if (has_any(cap, {"@mathml_nnc_xsd", "mathml_nnc_xsd", "@mathml", "mathml", "@tensor_math", "tensor_math"}))
        role = "mathml_nnc_xsd_manifest";
    if (has_any(cap, {"@xcfe_js_surfaces", "xcfe_js_surfaces", "@role_reversal", "role_reversal", "@modules", "modules"}))
        role = "xcfe_js_surface_manifest";
    if (has_any(cap, {"@kuhul_math_engine", "kuhul_math_engine", "@glyph_registry", "glyph_registry", "@executor_policy", "executor_policy"}))
        role = "kuhul_math_engine_manifest";
    if (has_any(cap, {"@agl_xjson_contracts", "agl_xjson_contracts", "@reserved_keys", "reserved_keys"}))
        role = "agl_xjson_contract_manifest";
    if (has_any(cap, {"@xjson_kuhul_merge", "xjson_kuhul_merge",
                      "@core_merge_architecture", "core_merge_architecture",
                      "@autonomous_ai_pipeline_xjson", "autonomous_ai_pipeline_xjson",
                      "@geometric_execution_engine", "geometric_execution_engine",
                      "@autonomous_agent_xjson", "autonomous_agent_xjson",
                      "@direct_file_ops_no_chat", "direct_file_ops_no_chat",
                      "@continuous_improvement_cycle", "continuous_improvement_cycle"}))
        role = "xjson_kuhul_autonomous_ai_runtime_manifest";
    if (has_any(cap, {"@kuhul_pi_virtual_cluster", "kuhul_pi_virtual_cluster", "@svg_model_distribution", "svg_model_distribution", "@cluster_nodes", "cluster_nodes"}))
        role = "kuhul_pi_virtual_cluster_manifest";
    if (has_any(cap, {"@kuhul_cluster_analytics", "kuhul_cluster_analytics", "@archetypes", "archetypes", "@analytics", "analytics"}))
        role = "kuhul_cluster_analytics_manifest";
    if (has_any(cap, {"@kuhul_character_engine", "kuhul_character_engine", "@character_archetypes", "character_archetypes", "@virtual_pi_runtime", "virtual_pi_runtime"}))
        role = "kuhul_character_engine_manifest";
    if (has_any(cap, {"@kuhul_learning_browser", "kuhul_learning_browser", "@learning_brain", "learning_brain", "@knowledge_graph", "knowledge_graph", "@attention_network", "attention_network"}))
        role = "kuhul_learning_browser_manifest";
    if (has_any(cap, {"@kuhul_auto_coder", "kuhul_auto_coder"}))
        role = "kuhul_auto_coder_manifest";
    if (has_any(cap, {"@ai_specialists_team", "ai_specialists_team", "@specialists", "specialists"}))
        role = "ai_specialists_team_manifest";
    if (has_any(cap, {"@mx2lm_runtime_boot", "mx2lm_runtime_boot", "@boot_stack", "boot_stack", "@glyph_stream", "glyph_stream"}))
        role = "mx2lm_runtime_boot_manifest";
    if (has_any(cap, {"@mx2lm_api_brain", "mx2lm_api_brain", "@api_brain_layers", "api_brain_layers", "@api_brain_services", "api_brain_services"}))
        role = "mx2lm_api_brain_manifest";
    if (has_any(cap, {"@bson", "bson", "@binary_runtime_array", "binary_runtime_array", "@glyph_lane_keys", "glyph_lane_keys"}))
        role = "bson_manifest";
    if (has_any(cap, {"@actions", "actions", "@commands", "commands", "@tools", "tools",
                      "@action_classes", "action_classes"}))
        role = "action_command_tool_manifest";
    if (has_any(cap, {"@experts", "experts", "@expert_pool", "expert_pool"}))
        role = "expert_pool_manifest";
    if (has_any(cap, {"@evolution", "evolution", "@mutation", "mutation", "@reward", "reward",
                      "@genome_contract", "genome_contract", "@opcode_surface", "opcode_surface"}))
        role = "evolution_manifest";

    return role;
}
