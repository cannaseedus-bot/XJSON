cmake_minimum_required(VERSION 3.10)
project(json_runtime)

set(CMAKE_CXX_STANDARD 17)

# Headers: nlohmann/json + cpp-httplib
include_directories(${CMAKE_SOURCE_DIR}/lib)

# ── Shared source sets ────────────────────────────────────────────────────────

set(XCFE_CORE
    src/xcfe.cpp
    src/scxq2.cpp
    src/gpu_dispatch.cpp
    src/tensor_runtime.cpp
    src/sco.cpp
    src/sidecar.cpp
    src/bots.cpp
    src/discovery.cpp
    src/file_system.cpp
    src/ops/op_add.cpp
    src/ops/op_print.cpp
    src/sw.cpp
)

# ── json_runtime (full XCFE + HTTP server) ────────────────────────────────────

add_executable(json_runtime
    src/main.cpp
    src/runtime.cpp
    src/runtime_authority.cpp
    src/server.cpp
    ${XCFE_CORE}
)

# ── sco_runtime (SCO registry + resolver + REST API) ─────────────────────────

add_executable(sco_runtime
    src/sco_main.cpp
    src/sco.cpp
    src/gpu_dispatch.cpp
    src/tensor_runtime.cpp
    src/xcfe.cpp
    src/scxq2.cpp
    src/sidecar.cpp
    src/bots.cpp
    src/file_system.cpp
    src/ops/op_add.cpp
    src/ops/op_print.cpp
    src/sw.cpp
)

# ── scxq2_runtime (SCXQ2 codec CLI) ──────────────────────────────────────────

add_executable(scxq2_runtime
    src/scxq2_main.cpp
    src/scxq2.cpp
)

# ── sw (Sidecar Store CLI; SidecarStore also linked into the runtimes above) ──

add_executable(sw
    src/sw_main.cpp
    src/sw.cpp
)

# ── Windows link + defines ────────────────────────────────────────────────────

if(WIN32)
    set(WIN_DEFS _WIN32_WINNT=0x0601)

    target_link_libraries(json_runtime  ws2_32 d3dcompiler)
    target_compile_definitions(json_runtime  PRIVATE ${WIN_DEFS})

    target_link_libraries(sco_runtime   ws2_32 d3dcompiler)
    target_compile_definitions(sco_runtime   PRIVATE ${WIN_DEFS})

    target_compile_definitions(sw        PRIVATE ${WIN_DEFS})

    # scxq2_runtime is pure codec -- no sockets needed; sw spawns via _popen (CRT)
endif()
