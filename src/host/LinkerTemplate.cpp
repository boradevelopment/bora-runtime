// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: LinkerTemplate.h
 * Purpose: ?
 */
#pragma once
#include "wasmtime.hh"
#include <type_traits>
#include <iostream>

#include "logging/LogManager.h"
#include "logging/LogStream.h"
#include "wasm/WasmLibraryLinker.h"

// To determine the kind a type is [TODO: move?]
template <typename T>
constexpr wasm_valkind_t GetWasmKind() {
    if constexpr (std::is_integral_v<T> && sizeof(T) <= 4) return WASMTIME_I32;
    if constexpr (std::is_integral_v<T> && sizeof(T) == 8) return WASMTIME_I64;
    if constexpr (std::is_floating_point_v<T> && sizeof(T) == 4) return WASMTIME_F32;
    if constexpr (std::is_floating_point_v<T> && sizeof(T) == 8) return WASMTIME_F64;
    // Fallback: Treat raw pointers or unmapped configurations as standard I32/I64 addresses
    return (sizeof(void*) == 8) ? WASMTIME_I64 : WASMTIME_I32;
}

class LinkerTemplate
{
public:
// Helper to log errors safely
    static void check_error(wasmtime_error_t* error) {
        if (error) {
            wasm_name_t msg;
            wasmtime_error_message(error, &msg);
            std::cerr << "Wasmtime Linker Error: " << std::string(msg.data, msg.size) << "\n";
            wasm_name_delete(&msg);
            wasmtime_error_delete(error);
        }
    }

    template <typename Ret, typename... Args>
    static void Function(
        wasmtime_linker_t* linker,
        const char* ns,
        const char* funcName,
        Ret(*funcPtr)(wasmtime_caller_t*, wasmtime_context_t*, Args...)
    ) {
        constexpr size_t num_args = sizeof...(Args);
        wasm_valtype_vec_t param_vec;

        if constexpr (num_args > 0) {
            wasm_valtype_t* param_array[num_args] = { wasm_valtype_new(GetWasmKind<Args>())... };
            wasm_valtype_vec_new(&param_vec, num_args, param_array);
        } else {
            wasm_valtype_vec_new_empty(&param_vec);
        }

        // 2. Build the Results Vector dynamically
        constexpr bool is_void = std::is_void_v<Ret>;
        wasm_valtype_vec_t result_vec;

        if constexpr (!is_void) {
            wasm_valtype_t* result_array[1] = { wasm_valtype_new(GetWasmKind<Ret>()) };
            wasm_valtype_vec_new(&result_vec, 1, result_array);
        } else {
            wasm_valtype_vec_new_empty(&result_vec);
        }

        // 3. Create the function type signature using the vectors
        wasm_functype_t* func_type = wasm_functype_new(&param_vec, &result_vec);
        // 4. Wrap user's target function pointer inside a type-safe generic lambda envelope
        struct Captures { Ret(*f)(wasmtime_caller_t*, wasmtime_context_t*, Args...); };
        Captures* env_data = new Captures{ funcPtr };

        wasmtime_error_t* error = wasmtime_linker_define_func(
            linker,
            ns, strlen(ns),
            funcName, strlen(funcName),
            func_type,
            [](void* env, wasmtime_caller_t* caller, const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults) -> wasm_trap_t* {
                auto* internal = static_cast<Captures*>(env);
                wasmtime_context_t* context = wasmtime_caller_context(caller);

                // Compile-time unpack sequence index tracker
                auto unpack_and_call = [&]<size_t... Is>(std::index_sequence<Is...>) -> wasm_trap_t* {
                    try
                    {
                        if constexpr (std::is_void_v<Ret>) {
                            internal->f(caller, context,
                                static_cast<Args>(args[Is].kind == WASMTIME_I64 ? args[Is].of.i64 : args[Is].of.i32)...
                            );
                        } else {
                            Ret res = internal->f(caller, context,
                                static_cast<Args>(args[Is].kind == WASMTIME_I64 ? args[Is].of.i64 : args[Is].of.i32)...
                            );

                            // Set the structural result type safely
                            results[0].kind = GetWasmKind<Ret>();
                            if constexpr (sizeof(Ret) == 8) {
                                results[0].of.i64 = static_cast<int64_t>(res);
                            } else {
                                results[0].of.i32 = static_cast<int32_t>(res);
                            }
                        }
                    } catch (const std::exception& e) {
                // Dynamically convert standard C++ failures into concrete WebAssembly engine traps
                return wasmtime_trap_new(e.what(), strlen(e.what()));
    }
                    return nullptr;
                };

                return unpack_and_call(std::make_index_sequence<sizeof...(Args)>{});;
            },
            env_data,
            [](void* data) { delete static_cast<Captures*>(data); } // Automatic cleanup callback to prevent memory leaks
        );

        wasm_functype_delete(func_type);
        check_error(error);
    }

static void GlobalI64(wasmtime_linker_t* linker, const char* get_namespace, const char* str, int64_t int64, bool cond)
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

};
