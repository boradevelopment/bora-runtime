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
#include "../wasm/WasmTools.h"
#include "logging/LogManager.h"
#include "logging/LogStream.h"
#include "../wasm/WasmRegistry.h"


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
        Ret(*funcPtr)(const WasmRuntimeModule*, const WasmThreadInstance*, wasmtime_caller_t*, Args...)
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

        wasm_functype_t* func_type = wasm_functype_new(&param_vec, &result_vec);
        struct Captures { Ret(*f)(const WasmRuntimeModule*, const WasmThreadInstance*, wasmtime_caller_t*,  Args...); };
        auto* env_data = new Captures{ funcPtr };

        wasmtime_error_t* error = wasmtime_linker_define_func(
            linker,
            ns, strlen(ns),
            funcName, strlen(funcName),
            func_type,
            [](void* env, wasmtime_caller_t* caller, const wasmtime_val_t* args, size_t nargs, wasmtime_val_t* results, size_t nresults) -> wasm_trap_t* {
                auto* internal = static_cast<Captures*>(env);
                wasmtime_context_t* context = wasmtime_caller_context(caller);

                // Get Module ID from here
                wasmtime_extern_t export_item;
                bool found = wasmtime_caller_export_get(caller, "module_id", 9, &export_item);

                if (!found || export_item.kind != WASMTIME_EXTERN_FUNC) {
                    return wasmtime_trap_new("Failed to resolve module_id export from guest", 45);
                }

                // 2. Call the guest's module_id() function to get the memory pointer
                wasmtime_val_t id_result;
                wasm_trap_t* trap = nullptr;
                wasmtime_error_t* error = nullptr;
                auto future = wasmtime_func_call_async(context, &export_item.of.func, nullptr, 0, &id_result, 1, &trap, &error);
                while (!wasmtime_call_future_poll(future)) {
                    std::this_thread::yield();
                }
                wasmtime_call_future_delete(future);
                if (trap) return trap;
                if (error) {
                    // Handle or free error appropriately
                    return wasmtime_trap_new("Error calling module_id", 23);
                }
                u64 strPtr = id_result.of.i64;
                auto moduleID = WasmTools::fromWASM<const char*>(caller, strPtr);
                auto threadInstance = WasmRegistry::Instance().GetThread(moduleID, context);
                auto module = WasmRegistry::Instance().Get(moduleID);

                if (!module)
                {
                    std::string msg = std::format("Module {} does not exist in the registry.", moduleID);
                    return wasmtime_trap_new(msg.c_str(), msg.length());
                }

                // Compile-time unpack sequence index tracker
                auto unpack_and_call = [&]<size_t... Is>(std::index_sequence<Is...>) -> wasm_trap_t* {
                    try
                    {
                        if constexpr (std::is_void_v<Ret>) {
                            internal->f(module, threadInstance, caller,
                                static_cast<Args>(args[Is].kind == WASMTIME_I64 ? args[Is].of.i64 : args[Is].of.i32)...
                            );
                        } else {
                            Ret res = internal->f(module, threadInstance, caller,
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
        wasm_valtype_vec_delete(&result_vec);
        wasm_valtype_vec_delete(&param_vec);

        check_error(error);
    }

static wasmtime_global_t GlobalI64(wasmtime_linker_t* linker, const char* get_namespace, const char* str, int64_t int64, bool cond, wasmtime_context_t* context = nullptr);
};
