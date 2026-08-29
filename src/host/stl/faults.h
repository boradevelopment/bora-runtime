// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: faults.h
 * Purpose: Symbols for FAULTS!
 */
#pragma once
#include "hostSymbolTemplate.hpp"
#include "wasm/WasmLibraryLinker.h"

static void memory_growth(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 memoryGrown) {

}

// This will throw an exception inside the wasm
static void segfault_handler(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext) {
    std::cerr << "[WASM FATAL]: Segfault! The Wasm module tried to access an invalid memory address." << std::endl;
    throw std::exception( "unreachable: segfault (host enforced)");

}

static void alignfault_handler(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext) {
    std::cerr << "[WASM FATAL]: Alignment Fault! The Wasm module performed an unaligned memory access." << std::endl;
    throw std::exception( "unreachable: alignfault (host enforced)");
}


static int32_t main_argc_argv_handler(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, int32_t argc, int64_t argv) {
    // If your host doesn't pass command line arguments to the module, return 0 (success)
    return 0;
}

static int32_t cxa_atexit_handler(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, int64_t dtor, int64_t arg, int64_t dso) {
    // Return 0 to signal to the WASM module that static destructor registration succeeded
    return 0;
}

// Handles sized operator delete: operator delete(void* ptr, size_t size)
static void operator_delete_sized_handler(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, int64_t ptr, int64_t size) {
    // Memory deallocation fallback. If your WASM linear memory heap is managed by
    // an internal allocator (like dlmalloc/sbrk), this can safely remain a no-op
    // or route to your shared heap manager.
    // todo
    // WasmLibraryLinker::getInstance().get_global_shared_memory()
}

// Handles aligned operator delete: operator delete(void* ptr, size_t size, std::align_val_t alignment)
static void operator_delete_aligned_handler(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, int64_t ptr, int64_t size, int64_t alignment) {
    // Aligned memory deallocation fallback.
}

// Handles (func $puts (import "env" "puts") (param i64) (result i32))
static int32_t puts_handler(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, int64_t str_ptr) {
    if (str_ptr == 0) return -1;

    const wasmtime_memory_t* memory = WasmLibraryLinker::getInstance()->getGlobalSharedMemory();
    uint8_t* memory_data = wasmtime_memory_data(moduleContext, memory);
    size_t memory_size = wasmtime_memory_data_size(moduleContext, memory);

    if (static_cast<size_t>(str_ptr) < memory_size) {
        const char* str = reinterpret_cast<const char*>(memory_data + str_ptr);
        int32_t ret = std::puts(str);
        std::fflush(stdout);
        return ret;
    }

    return -1;
}

// Handles (func $iprintf (import "env" "iprintf") (param i64 i64) (result i32))
static int32_t iprintf_handler(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, int64_t fmt_ptr, int64_t arg_val) {
    if (fmt_ptr == 0) return -1;

    const wasmtime_memory_t* memory = WasmLibraryLinker::getInstance()->getGlobalSharedMemory();
    uint8_t* memory_data = wasmtime_memory_data(moduleContext, memory);
    size_t memory_size = wasmtime_memory_data_size(moduleContext, memory);

    if (static_cast<size_t>(fmt_ptr) < memory_size) {
        const char* fmt = reinterpret_cast<const char*>(memory_data + fmt_ptr);
        int32_t ret = 0;

        // Check if second argument points to a string parameter in memory when format string contains %s
        if (arg_val >= 0 && static_cast<size_t>(arg_val) < memory_size && std::strstr(fmt, "%s")) {
            const char* str_arg = reinterpret_cast<const char*>(memory_data + arg_val);
            ret = std::printf(fmt, str_arg);
        } else {
            ret = std::printf(fmt, arg_val);
        }
        std::fflush(stdout);
        return ret;
    }

    return -1;
}

class EnvFaultHostSymbols : public HostSymbolTemplate {
public:
    const char* get_namespace() const override { return "env"; }

    void bind_symbols(wasmtime_linker_t* linker) const override
    {
        const char* ns = get_namespace();
        LinkerTemplate::Function(linker, ns, "segfault", segfault_handler);
        LinkerTemplate::Function(linker, ns, "alignfault", alignfault_handler);
        LinkerTemplate::Function(linker, ns, "emscripten_notify_memory_growth", memory_growth);
        LinkerTemplate::Function(linker, ns, "__main_argc_argv", main_argc_argv_handler);

        LinkerTemplate::Function(linker, ns, "puts", puts_handler);
        LinkerTemplate::Function(linker, ns, "iprintf", iprintf_handler);

        LinkerTemplate::Function(linker, ns, "__cxa_atexit", cxa_atexit_handler);
        LinkerTemplate::Function(linker, ns, "_ZdlPvm", operator_delete_sized_handler);
        LinkerTemplate::Function(linker, ns, "_ZdlPvmSt11align_val_t", operator_delete_aligned_handler);
    }
};
