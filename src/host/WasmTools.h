// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: WasmTools.h
 * Purpose: ?
 */

#pragma once
#include "wasm_export.h"

class WasmTools {
public:
    template <typename T>
    static T fromWASM(wasm_exec_env_t exec_env, u64 ptr) {
        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
        return (T)wasm_runtime_addr_app_to_native(module_inst, ptr);
    }
};
