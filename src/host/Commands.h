// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
/* 
 * FileName: Commands.h
 * Purpose: Provides host ends for commands
 */
#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include "hostSymbolTemplate.hpp"
#include "LinkerTemplate.h"
#include "WasmTools.h"

struct HostCommandListRecord {
    std::string name;
    std::string category;
    u64 wasmPtr;
    u64 size;
};

using OnInitializeHook = std::function<void(u64 handle, const HostCommandListRecord& record)>;
using OnUpdateHook = std::function<void(u64 handle, const HostCommandListRecord& record)>;

class HostCommandManager {
    static std::unordered_map<u64, HostCommandListRecord> m_activeLists;
    static u64 m_nextHandle;

    static std::vector<OnInitializeHook> m_initHooks;
    static std::vector<OnUpdateHook> m_updateHooks;
public:

    // Methods to add hooks from the Host side
     static void AddOnInitializeHook(const OnInitializeHook& hook) { m_initHooks.push_back(hook); }
     static void AddOnUpdateHook(const OnUpdateHook& hook) { m_updateHooks.push_back(hook); }

    // Called by Wasm IMPORT "initialize"
   static u64 Initialize(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 category_ptr, u64 name_ptr) {
         // Get the module instance from the execution environment
         const char* name = WasmTools::fromWASM<char*>(moduleCaller, moduleContext, name_ptr);
         const char* category = WasmTools::fromWASM<char*>(moduleCaller, moduleContext, category_ptr);
        u64 handle = m_nextHandle++;
        m_activeLists[handle] = { name, category, 0, 0 };
        for (auto& hook : m_initHooks) {    
            hook(handle, m_activeLists[handle]);
        }
        return handle;
    }

    // Called by Wasm IMPORT "update"
    static void Update(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 handle, u64 wasmPtr, u64 size) {
        auto it = m_activeLists.find(handle);
        if (it != m_activeLists.end()) {
            auto& record = it->second;
            record.wasmPtr = wasmPtr;
            record.size = size;

            // Trigger all Update hooks
            for (auto& hook : m_updateHooks) {
                hook(handle, record);
            }
        }
    }
};

class HostCommandSymbols :  public HostSymbolTemplate {
public:
    const char* get_namespace() const override {
        return "bora::commandList";
    }

    void bind_symbols(wasmtime_linker_t* linker) const override
    {
        const char* ns = get_namespace();
        LinkerTemplate::Function(linker, ns, "initialize", HostCommandManager::Initialize);
        LinkerTemplate::Function(linker, ns, "update", HostCommandManager::Update);
    }

};

