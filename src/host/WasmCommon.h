// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: WasmCommon.h
 * Purpose: Common stuff
 */

#pragma once
#include "wasmtime.hh"

struct WasmRuntimeModule {
    wasmtime_instance_t instance;
    wasmtime_table_t sharedTable;
    wasmtime_memory_t sharedMemory;
    wasmtime_global_t globalStack;
    wasmtime_global_t globalMemoryBase;
    wasmtime_global_t globalTableBase;
    wasmtime_module_t* module;
    std::vector<uint8_t> logoData;

    void destroy() const
    {
        if (module != nullptr) wasmtime_module_delete(module);
        delete this;
    }
};
