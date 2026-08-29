// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: WasmTools.h
 * Purpose: ?
 */

#pragma once
#include "wasm.h"
#include "wasmtime.hh"
#include "WasmCommon.h"
#include "wasm/WasmLibraryLinker.h"

struct ToolsContext
{
    wasmtime_context_t* lastContext = nullptr;
    WasmRuntimeModule* module = nullptr;
};

class WasmTools {
private:
    // Wasmtime tracks execution states using its store context.
    static inline ToolsContext lastContext;
public:
    static bool getFunctionByIndex(wasmtime_context_t* context, const WasmRuntimeModule* module, uint32_t target_index, wasmtime_func_t* out_func) {
        wasm_exporttype_vec_t export_types;
        wasmtime_module_exports(module->module, &export_types);
        uint32_t current_func_index = 0;
        bool found = false;
        for (size_t i = 0; i < export_types.size; ++i) {
            // Check if the export entry is a function
            const wasm_externtype_t* extern_type = wasm_exporttype_type(export_types.data[i]);
            if (wasm_externtype_kind(extern_type) == WASM_EXTERN_FUNC) {

                // If this is the function index we are looking for...
                if (current_func_index == target_index) {
                    // Fetch the actual runtime external item
                    wasmtime_extern_t item;
                    const wasm_name_t* name_vec = wasm_exporttype_name(export_types.data[i]);
                    wasmtime_instance_export_get(context, &module->instance, name_vec->data, name_vec->size, &item);

                    if (item.kind == WASMTIME_EXTERN_FUNC) {
                        *out_func = item.of.func;
                        found = true;
                        break;
                    }
                }
                current_func_index++;
            }
        }

        // Clean up the vector container allocations
        wasm_exporttype_vec_delete(&export_types);
        return found;
    }

    static bool doesFunctionExistInInstance(wasmtime_context_t* context, const WasmRuntimeModule* wasmModule, const char* functionName)
    {
        wasmtime_extern_t item;
        bool found = wasmtime_instance_export_get(
            context,
            &wasmModule->instance,
            functionName,
            strlen(functionName),
            &item
        );

        return found && item.kind == WASMTIME_EXTERN_FUNC;
    }

    static bool getFunctionInInstance(wasmtime_context_t* context, const WasmRuntimeModule* wasmModule, const char* functionName, wasmtime_func_t* function)
    {
        wasmtime_extern_t item;
        bool found = wasmtime_instance_export_get(
            context,
            &wasmModule->instance,
            functionName,
            strlen(functionName),
            &item
        );

        bool success = found && item.kind == WASMTIME_EXTERN_FUNC;
        if (success)
        {
            *function = item.of.func;
        }

        return success;
    }

    static void setLastContext(wasmtime_context_t* context, WasmRuntimeModule* module = nullptr) {
        lastContext.lastContext = context;
        lastContext.module = module;
    }
    static ToolsContext* getLastContext() {
        return &lastContext;
    }

    // Helper to extract default memory out of an active instance
    static bool getMemory(wasmtime_context_t* context, const wasmtime_instance_t* instance, wasmtime_memory_t* out_memory) {
        wasmtime_extern_t item;
        bool found = wasmtime_instance_export_get(context, const_cast<wasmtime_instance_t*>(instance), "memory", 6, &item);
        if (found && item.kind == WASMTIME_EXTERN_MEMORY) {
            *out_memory = item.of.memory;
            return true;
        }

        auto* linker = WasmLibraryLinker::getInstance();
        if (linker != nullptr)
        {
            auto* shared_mem_ptr = linker->getGlobalSharedMemory();
            if (shared_mem_ptr != nullptr) {
                *out_memory = *shared_mem_ptr;
                return true;
            }
        }

        return false;
    }

    static bool getMemory(wasmtime_caller_t* caller, wasmtime_memory_t* out_memory) {
        wasmtime_extern_t item;
        bool found = wasmtime_caller_export_get(caller, "memory", 6, &item);
        if (found && item.kind == WASMTIME_EXTERN_MEMORY) {
            *out_memory = item.of.memory;
            return true;
        }

        auto* linker = WasmLibraryLinker::getInstance();
        if (linker != nullptr)
        {
            auto* shared_mem_ptr = linker->getGlobalSharedMemory();
            if (shared_mem_ptr != nullptr) {
                *out_memory = *shared_mem_ptr;
                return true;
            }
        }


        return false;
    }


    // Convert Guest WASM pointer offset to Host Native Pointer
    template <typename T>
    static T fromWASM(wasmtime_context_t* context, const WasmRuntimeModule* module, u64 ptr) {
        setLastContext(context, const_cast<WasmRuntimeModule*>(module));
        wasmtime_memory_t memory;
        if (!getMemory(context, &module->instance, &memory)) return nullptr;

        uint8_t *base = wasmtime_memory_data(context, &memory);
        size_t size = wasmtime_memory_data_size(context, &memory);

        if (ptr < size) {
            return reinterpret_cast<T>(base + ptr);
        }
        std::cerr << "WasmTools Error: Guest pointer out of bounds!\n";
        return nullptr;
    }

    template <typename T>
   static T fromWASM(wasmtime_caller* caller, wasmtime_context_t* context, u64 ptr) {
        setLastContext(context);
        wasmtime_memory_t memory;
        if (!getMemory(caller, &memory)) return nullptr;

        uint8_t *base = wasmtime_memory_data(context, &memory);
        size_t size = wasmtime_memory_data_size(context, &memory);

        if (ptr < size) {
            return reinterpret_cast<T>(base + ptr);
        }
        std::cerr << "WasmTools Error: Guest pointer out of bounds!\n";
        return nullptr;
    }

    // Convert Host Native Pointer to Guest WASM pointer offset
    static u64 toWASM(wasmtime_context_t* context, const WasmRuntimeModule* module, void* native_ptr) {
        if (!native_ptr) return 0;
        setLastContext(context);

        wasmtime_memory_t memory;
        if (!getMemory(context, &module->instance, &memory)) return 0;

        uint8_t *base = wasmtime_memory_data(context, &memory);
        size_t size = wasmtime_memory_data_size(context, &memory);

        uint8_t *target = static_cast<uint8_t*>(native_ptr);
        if (target >= base && target < (base + size)) {
            return static_cast<u64>(target - base);
        }
        std::cerr << "WasmTools Error: Native address is not inside WASM memory space!\n";
        return 0;
    }

    static u64 toWASM(wasmtime_caller_t* caller, wasmtime_context_t* context, void* native_ptr) {
        if (!native_ptr) return 0;
        setLastContext(context);

        wasmtime_memory_t memory;
        if (!getMemory(caller, &memory)) return 0;

        uint8_t *base = wasmtime_memory_data(context, &memory);
        size_t size = wasmtime_memory_data_size(context, &memory);

        uint8_t *target = static_cast<uint8_t*>(native_ptr);
        if (target >= base && target < (base + size)) {
            return static_cast<u64>(target - base);
        }
        std::cerr << "WasmTools Error: Native address is not inside WASM memory space!\n";
        return 0;
    }

    // Variadic Template helper to cast standard C++ arguments into explicit typed Wasmtime Value structures
    template <typename Arg>
    static wasmtime_val_t convertToWasmValue(Arg arg) {
        wasmtime_val_t val;
        if constexpr (std::is_integral_v<Arg> && sizeof(Arg) <= 4) {
            val.kind = WASMTIME_I32;
            val.of.i32 = static_cast<int32_t>(arg);
        } else if constexpr (std::is_integral_v<Arg> && sizeof(Arg) == 8) {
            val.kind = WASMTIME_I64;
            val.of.i64 = static_cast<int64_t>(arg);
        } else if constexpr (std::is_floating_point_v<Arg> && sizeof(Arg) == 4) {
            val.kind = WASMTIME_F32;
            val.of.f32 = static_cast<float>(arg);
        } else if constexpr (std::is_floating_point_v<Arg> && sizeof(Arg) == 8) {
            val.kind = WASMTIME_F64;
            val.of.f64 = static_cast<double>(arg);
        } else {
            // Treat pointers or other types as explicit I32 addresses
            val.kind = WASMTIME_I32;
            val.of.i32 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(arg));
        }
        return val;
    }

    template <typename T, typename... Args>
    static T CallWASMFunction(wasmtime_context_t* context, const WasmRuntimeModule* module, wasmtime_func_t func, wasm_trap_t** trapO = nullptr, wasmtime_error_t** errorO = nullptr, Args... args)
    {
        wasmtime_error_t* error = nullptr;
        wasm_trap_t* trap = nullptr;

        if (trapO != nullptr) *trapO = trap;
        if (errorO != nullptr) *errorO = error;

        std::vector<wasmtime_val_t> wasm_args = { convertToWasmValue(args)... };
        constexpr size_t ret_count = std::is_void_v<T> ? 0 : 1;
        wasmtime_val_t results[1];

        // if (WasmLibraryLinker::getInstance()->getDebugging())
        // {
        //     std::vector<std::string> string_storage = { [](const auto& arg) -> std::string {
        //         using ArgType = std::decay_t<decltype(arg)>;
        //         if constexpr (std::is_same_v<ArgType, const char*> || std::is_same_v<ArgType, char*>) {
        //             return std::string(arg);
        //         } else if constexpr (std::is_same_v<ArgType, std::string>) {
        //             return arg;
        //         } else {
        //             return std::to_string(arg);
        //         }
        //     }(args)... };
        //
        //     std::vector<const char*> arg_ptrs;
        //     arg_ptrs.reserve(string_storage.size());
        //     for (const auto& str : string_storage) {
        //         arg_ptrs.push_back(str.c_str());
        //     }
        //     error = bwasmtime_gdb_call_func(WasmLibraryLinker::getInstance()->getDebugServer(), WasmLibraryLinker::getInstance()->getStore(), &func, arg_ptrs.data(), wasm_args.size(), results, nullptr, 0);
        // } else
        // {
            auto future = wasmtime_func_call_async(context, &func, wasm_args.data(), wasm_args.size(), results, ret_count, &trap, &error);
            while (!wasmtime_call_future_poll(future)) {
                // Optionally yield thread/task while waiting
                std::this_thread::yield();
            }
           wasmtime_call_future_delete(future);
        // }

        if (error != nullptr || trap != nullptr) {
            return T();
        }

        if constexpr (std::is_void_v<T>) {
            return;
        } else {
            if constexpr (std::is_pointer_v<T>) {
                // Only for really basic types currently.
                if (results[0].kind == WASMTIME_I32) {
                    return reinterpret_cast<T>(static_cast<uintptr_t>(results[0].of.i32));
                }
                if (results[0].kind == WASMTIME_I64) {
                    return reinterpret_cast<T>(static_cast<uintptr_t>(results[0].of.i64));
                }
            } else if constexpr (std::is_floating_point_v<T>) {
                // Safely handles float and double without casting to uintptr_t
                if constexpr (sizeof(T) == 4) {
                    if (results[0].kind == WASMTIME_F32) return static_cast<T>(results[0].of.f32);
                } else {
                    if (results[0].kind == WASMTIME_F64) return static_cast<T>(results[0].of.f64);
                }
            } else if constexpr (std::is_integral_v<T>) {
                // Handles integers (int, uint32_t, u64, etc.)
                if constexpr (sizeof(T) == 8) {
                    if (results[0].kind == WASMTIME_I64) return static_cast<T>(results[0].of.i64);
                } else {
                    if (results[0].kind == WASMTIME_I32) return static_cast<T>(results[0].of.i32);
                }
            }

            return T();
        }
    }

    template <typename T, typename... Args>
    static T RequestExportedMethod(wasmtime_context_t* context, const WasmRuntimeModule* module, const char* methodName, wasm_trap_t** trap = nullptr, wasmtime_error_t** error = nullptr, Args... args) {
        setLastContext(context);

        // 1. Find the method
        wasmtime_extern_t item;
        bool found = wasmtime_instance_export_get(context, &module->instance, methodName, strlen(methodName), &item);
        if (!found || item.kind != WASMTIME_EXTERN_FUNC) {
            std::cerr << "Error: Exported method '" << methodName << "' not found!\n";
            return T();
        }
        return CallWASMFunction<T>(context, module, item.of.func, trap, error, args...);

    }

    template <typename... Args>
    static bool CallExportedMethod(wasmtime_context_t* context, WasmRuntimeModule* module, const char* methodName, wasm_trap_t** trap = nullptr, wasmtime_error_t** error = nullptr, Args... args) {
        // Void extraction call maps directly into RequestExportedMethod
        RequestExportedMethod<void>(context, module, methodName, trap, error, args...);
        return true;
    }

    template<typename T, typename... Args>
    static T CallByIndex(wasmtime_context_t* context, const WasmRuntimeModule* module, uint32_t func_index, wasm_trap_t** trap = nullptr, wasmtime_error_t** error = nullptr, Args... args) {
        wasmtime_func_t target_func;
        getFunctionByIndex(context, module, func_index, &target_func);
        return CallWASMFunction<T>(context, module, target_func, trap, error, args...);
    }

    template<typename T, typename... Args> // improvise on lastContext - possibly use IDS to identity a Runtime Module!
    static T CallByIndex(uint32_t func_index, wasm_trap_t** trap = nullptr, wasmtime_error_t** error = nullptr, Args... args) {
        wasmtime_func_t target_func;
        getFunctionByIndex(lastContext.lastContext, lastContext.module, func_index, &target_func);

        return CallWASMFunction<T>(lastContext.lastContext, lastContext.module, target_func, trap,error, args...);
    }

    static bool logandDeleteWASMFail(wasmtime_error_t* error, wasm_trap_t* trap, const sString& name)
    {
        auto logInstance = LogManager::instance().getLogger("bora.module.execution");

        if (error != nullptr || trap != nullptr) {
            if (error)
            {
                wasm_name_t msg;
                wasmtime_error_message(error, &msg);
                LOG_ERROR_VERBOSE(logInstance, 1) << (name.empty() ? "" : name + " ") << "Error: " << std::string(msg.data, msg.size);
                wasm_name_delete(&msg);
                wasmtime_error_delete(error);
            }
            if (trap)
            {
                if (WasmLibraryLinker::getInstance()->getDebugging())
                {
                    wasm_trap_delete(trap);
                    trap = nullptr;
                    wasmtime_context_set_epoch_deadline(WasmLibraryLinker::getInstance()->getContext(), 100);
                    return false;
                }
                wasm_name_t msg;
                wasm_trap_message(trap, &msg);
                LOG_ERROR_VERBOSE(logInstance, 1) << "Trap for " << (name.empty() ? "" : name) << ": " << std::string(msg.data, msg.size) << "\n";
                wasm_name_delete(&msg);
            }
            return true;
        }

        return false;
    }

    static bool logandDeleteWASMFail(wasmtime_error_t* error, const sString& name)
    {
        auto logInstance = LogManager::instance().getLogger("bora.module.execution");

        if (error != nullptr) {
            wasm_name_t msg;
            wasmtime_error_message(error, &msg);
            LOG_ERROR_VERBOSE(logInstance, 1) << (name.empty() ? "" : name + " ") << "Error: " << std::string(msg.data, msg.size);
            wasm_name_delete(&msg);
            wasmtime_error_delete(error);
        }

        return false;
    }
};