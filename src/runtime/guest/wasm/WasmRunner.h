// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: WasmRunner.h
 * Purpose: Setups a WASM module.
 */

#pragma once
#include "wasmtime.hh"
#include "TAZA.h"
#include "guest/guestImports.h"
#include "guest/wasm/WasmCommon.h"

class WasmGlobal;

class WasmRunner {
 public:
 static inline wasm_limits_t appMemoryLimits;

 static void initalizeWAMRMemoryCallbacks();
 static void clearCache();

 /// Loads the WASM using the TAZA format which all bora applications standardly used.
 static const WasmRuntimeModule* loadModuleByTAZA(WasmGlobal* linker, wasmtime_store_t* store, wasmtime_context_t* context, wasm_engine_t *engine, const char* tazaFileName, wasmtime_sharedmemory_t* shared_memory = nullptr, const wasmtime_global_t* globalStack = nullptr, bool isRecursed = false);
 static const WasmThreadInstance* createThreadInstance(WasmGlobal* linker, wasmtime_store_t* store, wasmtime_context_t* context, const WasmRuntimeModule *engine, u64 stackOffset);
 /// Loads the WASM using the standard IO stream, which I don't have a idea which use case this would be used for except for testing
 static const WasmRuntimeModule* loadModuleByFilename(WasmGlobal linker, wasmtime_context_t* context, wasm_engine_t *engine, const char* fileName);
 /// Loads the WASM through memory for maybe built in WASM binaries.
 static const WasmRuntimeModule* loadModuleByMemory(WasmGlobal linker, wasmtime_context_t* context, wasm_engine_t *engine, u8* data, u64 size);
};


