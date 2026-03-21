// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: WasmTools.h
 * Purpose: ?
 */

#pragma once
#include "wasm_export.h"

class WasmTools {
private:
    static inline wasm_exec_env_t lastExecutionEnvironment;
public:
    static void setLastExecutionEnvironment(wasm_exec_env_t env) {
        lastExecutionEnvironment = env;
    }
    static wasm_exec_env_t getLastExecutionEnvironment() {
        return lastExecutionEnvironment;
    }
    template <typename T>
    static T fromWASM(wasm_exec_env_t exec_env, u64 ptr) {
        setLastExecutionEnvironment(exec_env);
        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
        return (T)wasm_runtime_addr_app_to_native(module_inst, ptr);
    }


    static u64 toWASM(wasm_exec_env_t exec_env, void* native_ptr) {
        if (!native_ptr) return 0;

        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);

        // This returns the offset relative to the start of the WASM linear memory
        return (u64)wasm_runtime_addr_native_to_app(module_inst, native_ptr);
    }

    static u64 toWASM(wasm_module_inst_t wasmModule, void* native_ptr) {
        if (!native_ptr) return 0;
        return (u64)wasm_runtime_addr_native_to_app(wasmModule, native_ptr);
    }

    template <typename T>
   static T fromWASM(wasm_module_inst_t wasmModule, u64 ptr) {
        return (T)wasm_runtime_addr_app_to_native(wasmModule, ptr);
    }

    template <typename T, typename... Args>
    static T RequestExportedMethod(wasm_exec_env_t exec_env, const char* methodName, Args... args) {
        setLastExecutionEnvironment(exec_env);
        // 1. Determine slot requirements
        constexpr size_t arg_slots = sizeof...(args);
        constexpr size_t ret_slots = (sizeof(T) + 3) / 4;
        // argv must be large enough for the larger of the two
        constexpr size_t total_slots = (arg_slots > ret_slots) ? arg_slots : ret_slots;

        // 2. Initialize argv buffer
        uint32_t argv[total_slots > 0 ? total_slots : 1] = { static_cast<uint32_t>(args)... };

        // 3. Find and Call
        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
        wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, methodName);

        if (!func) {
            printf("Error: Exported method '%s' not found!\n", methodName);
            return T();
        }

        if (!wasm_runtime_call_wasm(exec_env, func, arg_slots, argv)) {
            printf("WASM Exception in %s: %s\n", methodName, wasm_runtime_get_exception(module_inst));
            return T();
        }

        // 4. Extract Result
        if constexpr (std::is_void_v<T>) {
            return;
        } else if constexpr (sizeof(T) == 8) {
            T result;
            memcpy(&result, argv, 8);
            return result;
        } else {
            // Use uintptr_t to safely cast 32-bit wasm indices to pointers if needed
            return (T)(uintptr_t)argv[0];
        }
    }

    // High-level: Call a method with no return value (or ignore it)
    template <typename... Args>
    static bool CallExportedMethod(wasm_exec_env_t exec_env, const char* methodName, Args... args) {
        setLastExecutionEnvironment(exec_env);
        uint32_t argv[] = { static_cast<uint32_t>(args)... };

        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
        wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, methodName);

        if (!func) return false;
        return wasm_runtime_call_wasm(exec_env, func, sizeof...(args), argv);
    }

    template <typename T, typename... Args>
static T CallByIndex(wasm_exec_env_t exec_env, uint32_t func_idx, Args... args) {
        setLastExecutionEnvironment(exec_env);

        // Determine slots needed safely
        constexpr size_t arg_slots = sizeof...(args);

        // We use a dummy type (char) if T is void to satisfy the compiler's sizeof check
        using SafeT = std::conditional_t<std::is_void_v<T>, char, T>;
        constexpr size_t ret_slots = std::is_void_v<T> ? 0 : (sizeof(SafeT) + 3) / 4;

        constexpr size_t total_slots = (arg_slots > ret_slots) ? arg_slots : ret_slots;

        // Initialize argv
        uint32_t argv[total_slots > 0 ? total_slots : 1] = { static_cast<uint32_t>((u64)(args))... };

        bool success = wasm_runtime_call_indirect(exec_env, func_idx, arg_slots, argv);

        if (!success) {
            printf("Error: Indirect Call Failed For Index %u", func_idx);
            return T();
        }

        // 4. Extract Result
        if constexpr (std::is_void_v<T>) {
            return;
        } else if constexpr (sizeof(T) == 8) {
            T result;
            memcpy(&result, argv, 8);
            return result;
        } else {
            // Use uintptr_t to safely cast 32-bit wasm indices to pointers if needed
            return (T)(uintptr_t)argv[0];
        }

}
};
