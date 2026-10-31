// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: WasmCommon.h
 * Purpose: Common stuff
 */

#pragma once
#include <utility>

#include "IMappedMemory.h"
#include "wasmtime.hh"
#include "WasmRegistry.h"


struct WasmThreadInstance
{
    WasmThreadInstance(const std::string& moduleID, wasmtime_context_t* context, wasmtime_store_t* store) : moduleID(moduleID), context(context), store(store)
    {
        // Dependencies use the same context but different thread instances, ownsStore is a safety check
        ownsStore = !WasmRegistry::Instance().IsContextUsedInThread(context);

        WasmRegistry::Instance().RegisterThread(moduleID.c_str(), context, this);
    }

    ~WasmThreadInstance()
    {
       destroy();
    }

    void destroy() const
    {
        if (context) WasmRegistry::Instance().UnregisterThread(moduleID.c_str(), context);
        if (store && ownsStore) wasmtime_store_delete(store);
    }

    bool ownsStore = false;
    std::string moduleID;
    wasmtime_store_t* store{};
    wasmtime_context_t* context{};
    wasmtime_instance_t instance{};
    wasmtime_global_t globalStack{};
    wasmtime_global_t globalMemoryBase{};
    wasmtime_global_t globalTableBase{};
    wasmtime_table_t sharedTable{};
};

struct WasmRuntimeModule {
    WasmRuntimeModule(std::string appID, wasmtime_context_t* context, wasmtime_store_t* store): appId(std::move(appID)), mainThread(appID.c_str(), context, store)
    {
        WasmRegistry::Instance().Register(appId.c_str(), this);
    }

    WasmThreadInstance mainThread;
    wasmtime_sharedmemory_t* sharedMemory{};
    i64 memoryBaseValue{};
    i64 tableBaseValue{};
    wasmtime_module_t* module{};
    std::vector<uint8_t> logoData;
    std::string appId;
    sUniquePointer<IMappedMemory> archiveMap;

    void destroy() const
    {
        WasmRegistry::Instance().Unregister(appId.c_str());
        if (module != nullptr) wasmtime_module_delete(module);
        delete this;
    }
};
