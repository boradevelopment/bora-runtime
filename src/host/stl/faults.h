// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: faults.h
 * Purpose: Symbols for FAULTS!
 */
#pragma once
#include "hostSymbolTemplate.hpp"

static void memory_growth(wasm_exec_env_t exec_env, u64 memoryGrown) {
// printf("Memory grew to %llu", memoryGrown);
}

// This will throw an exception inside the wasm
static void segfault_handler(wasm_exec_env_t exec_env) {
    // 1. Log the error to your host console
    std::cerr << "[WASM FATAL]: Segfault! The Wasm module tried to access an invalid memory address." << std::endl;
    printf("Bora Runtime Stack Traceback:\n");
    wasm_runtime_dump_call_stack(exec_env);
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);

    wasm_runtime_set_exception(inst, "unreachable: segfault");
}

static void alignfault_handler(wasm_exec_env_t exec_env) {
    std::cerr << "[WASM FATAL]: Alignment Fault! The Wasm module performed an unaligned memory access." << std::endl;

    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    wasm_runtime_set_exception(inst, "unreachable: alignfault");
}

class EnvFaultHostSymbols : public HostSymbolTemplate {
    std::vector<NativeSymbol> symbols;
public:
    EnvFaultHostSymbols() {
        symbols = {
            {"segfault", (void*)segfault_handler, "()",  nullptr},
            {"alignfault", (void*)alignfault_handler, "()",  nullptr},
            {"emscripten_notify_memory_growth", (void*)memory_growth, "(I)",  nullptr}
        };
    }

    const char* get_namespace() const override { return "env"; }
    const std::vector<NativeSymbol>& get_symbols() const override { return symbols; }
};
