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
#include <utility>
#include "../templates/LinkerTemplate.h"
#include "../wasm/WasmTools.h"
#include "nCommon/CommandRegistry.h"
#include "../templates/hostSymbolTemplate.hpp"

struct HostCommandListRecord {
    std::string name;
    std::string category;
    u64 wasmPtr{};
    u64 size{};
    const WasmRuntimeModule* module = nullptr;
    const WasmThreadInstance* instance = nullptr;
};

class WasmCommandProxy : public ICommand {
private:
    const void* payload;
    std::string name;
    std::string category;
    u64 executeStaticPtr_;

public:
    WasmCommandProxy(std::string name, std::string category, const void* payload, u64 objPtr)
        : executeStaticPtr_(objPtr), payload(payload), name(std::move(name)), category(std::move(category)) {}

    // context will always be null.
    void ExecuteStatic(void* context, const WasmThreadInstance* threadContext = nullptr, const WasmRuntimeModule* wasmModule = nullptr) override {
        wasm_trap_t* trap = nullptr;
        wasmtime_error_t* error = nullptr;
        // payloads are automatically converted back to native memory in the hook, however, we need it back to standard WASM indexes.
        WasmTools::CallByIndex<void>(threadContext, wasmModule, executeStaticPtr_, &trap, &error, WasmTools::toWASM(threadContext, wasmModule, const_cast<void*>(payload)));
        auto id = std::format("{} Command List : {} (Registered on the {} module)", category, name, wasmModule->appId);
        WasmTools::logandDeleteWASMFail(error, trap, id);
    }
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
   static u64 Initialize(const WasmRuntimeModule* module, const WasmThreadInstance* threadContext, wasmtime_caller_t* moduleCaller,  u64 category_ptr, u64 name_ptr) {
         // Get the module instance from the execution environment
         const char* name = WasmTools::fromWASM<char*>(moduleCaller, name_ptr);
         const char* category = WasmTools::fromWASM<char*>(moduleCaller, category_ptr);
        u64 handle = m_nextHandle++;
        m_activeLists[handle] = { name, category, 0, 0, module, threadContext };
        for (auto& hook : m_initHooks) {    
            hook(handle, m_activeLists[handle]);
        }
        return handle;
    }

    // Called by Wasm IMPORT "update"
    static void Update(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, u64 handle, u64 wasmPtr, u64 size) {
        auto it = m_activeLists.find(handle);
        if (it != m_activeLists.end()) {
            auto& record = it->second;
            record.module = module;
            record.instance = threadInstance;
            record.wasmPtr = wasmPtr;
            record.size = size;

            // Trigger all Update hooks
            for (auto& hook : m_updateHooks) {
                hook(handle, record);
            }
        }
    }

    static void RegisterCommand(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller , u64 categoryPtr, u64 namePtr, u64 registerPtr)
     {
         if (!categoryPtr || !namePtr || !registerPtr) return;

         auto category = WasmTools::fromWASM<const char*>(moduleCaller, categoryPtr);
         auto name = WasmTools::fromWASM<const char*>(moduleCaller, namePtr);

         auto commandID = MakeCommandID(category, name);
         if (CommandRegistry::Instance().GetFactories()->contains(commandID))
         { // safety reasons
             LOG_ERROR(LogManager::instance().getLogger("bora.commands")) << "Command ID " << commandID << " already registered. Register will be ignored.";
             return;
         }
          CommandRegistry::Instance().Register(commandID,[=](const void* payload) -> ICommand* {
             return new WasmCommandProxy(name, category, payload, registerPtr);
         });
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
        LinkerTemplate::Function(linker, ns, "register", HostCommandManager::RegisterCommand);
    }

};

