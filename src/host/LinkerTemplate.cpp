// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
#include "LinkerTemplate.h"

#include <cstring>

#include "wasm/WasmLibraryLinker.h"

void LinkerTemplate::GlobalI64(wasmtime_linker_t* linker, const char* get_namespace, const char* str, int64_t int64,
    bool cond)
{
    if (!linker || !get_namespace || !str) {
        return;
    }
    auto logInstance = LogManager::instance().getLogger("bora.module.linker");

    // 1. Retrieve the active Wasmtime Store/Context required for object creation
    // (Adjust this line to match how your engine retrieves the active context)
    wasmtime_context_t* context = WasmLibraryLinker::getInstance()->getContext();
    if (!context) {
        return;
    }

    wasm_valtype_t* val_type = wasm_valtype_new(WASM_I64);
    wasm_mutability_t mutability = cond ? WASM_VAR : WASM_CONST;
    wasm_globaltype_t* global_type = wasm_globaltype_new(val_type, mutability);

    wasmtime_val_t init_val;
    init_val.kind = WASMTIME_I64;
    init_val.of.i64 = int64;

    wasmtime_global_t global_instance;
    wasmtime_error_t* error = wasmtime_global_new(
        context,
        global_type,
        &init_val,
        &global_instance
    );

    // clean up type descriptor after creation
    wasm_globaltype_delete(global_type);

    if (error != nullptr) {
        // Log error and free error pointer
        LOG_ERROR(logInstance) << "Failed to create wasmtime_global: " + std::string(str);
        wasmtime_error_delete(error);
        return;
    }

    // 5. Wrap the global in a wasmtime_extern_t struct
    wasmtime_extern_t global_extern;
    global_extern.kind = WASMTIME_EXTERN_GLOBAL;
    global_extern.of.global = global_instance;

    // 6. Define the global symbol in the Linker
    error = wasmtime_linker_define(
        linker,
        context,
        get_namespace,
        std::strlen(get_namespace),
        str,
        std::strlen(str),
        &global_extern
    );

    if (error != nullptr) {
        LOG_ERROR(logInstance) << "Failed to define global symbol in linker: " + std::string(get_namespace) + "." + std::string(str);
        wasmtime_error_delete(error);
    }
}
