// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: WasmRunner.h
 * Purpose: ?
 */

#pragma once
#include "wasmtime.hh"
#include "TAZA.h"
#include "host/hostSymbols.h"
#include "host/WasmCommon.h"

class WasmLibraryLinker;

class WasmRunner {
 public:
 static inline wasm_limits_t appMemoryLimits;

 static void initalizeWAMRMemoryCallbacks();
 static void clearCache();

 /// Loads the WASM using the TAZA format which all bora applications standardly used.
 static const WasmRuntimeModule* loadModuleByTAZA(WasmLibraryLinker* linker, wasmtime_context_t* context, wasm_engine_t *engine, const char* tazaFileName, const wasmtime_memory_t* shared_memory = nullptr, const wasmtime_global_t* globalStack = nullptr);
 /// Loads the WASM using the standard IO stream, which I don't have a idea which use case this would be used for except for testing
 static const WasmRuntimeModule* loadModuleByFilename(WasmLibraryLinker linker, wasmtime_context_t* context, wasm_engine_t *engine, const char* fileName);
 /// Loads the WASM through memory for maybe built in WASM binaries.
 static const WasmRuntimeModule* loadModuleByMemory(WasmLibraryLinker linker, wasmtime_context_t* context, wasm_engine_t *engine, u8* data, u64 size);
};


