// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: WasmRegistry.h
 * Purpose: Allows you to get a wasm module through a registry index.
*/
#pragma once
#include <wasmtime/store.h>

#include <utility>
#include "logging/LogManager.h"
#include "logging/LogStream.h"

struct WasmThreadInstance;
struct WasmRuntimeModule;

struct ThreadInformation
{
    std::string moduleID;
    wasmtime_context_t* context;

    ThreadInformation(std::string moduleID, wasmtime_context_t* context)
    {
        this->moduleID = std::move(moduleID);
        this->context = context;
    }

    bool operator==(const ThreadInformation& other) const {
        return moduleID == other.moduleID && context == other.context;
    }
};

template <>
struct std::hash<ThreadInformation> {
    size_t operator()(const ThreadInformation& info) const noexcept
    {
        // Combine the hash of the string and the pointer address
        size_t h1 = std::hash<std::string>{}(info.moduleID);
        size_t h2 = std::hash<void*>{}(static_cast<void*>(info.context));
        return h1 ^ (h2 << 1); // Simple bitwise combine
    }
};

class WasmRegistry
{
private:
    sUnorderedMap<std::string, const WasmRuntimeModule*> modules_;
    sUnorderedMap<ThreadInformation, const WasmThreadInstance*> threads_;
    mutable std::mutex registryMutex;
public:
    WasmRegistry() = default;

    static WasmRegistry& Instance()
    {
        static WasmRegistry instance;
        return instance;
    }

    void Unregister(const char* id)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        modules_.erase(id);
    }

    void Register(const char* id, const WasmRuntimeModule* module)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        if (modules_.contains(id))
        {
            LOG_ERROR(LogManager::instance().getLogger("bora.registry")) << "ID" << id << " already exists in registry!";
            return;
        }
        modules_.emplace(id, module);
    }

    void RegisterThread(const char* moduleID, wasmtime_context_t* context, const WasmThreadInstance* instance)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        if (instance)
        {
            if (auto it = threads_.find(ThreadInformation{moduleID, context}); it != threads_.end())
            {
                LOG_ERROR(LogManager::instance().getLogger("bora.registry")) << "Thread already exists in registry!";
                return;
            }


            threads_.emplace(ThreadInformation(moduleID, context), instance);
        }
    }

    void UnregisterThread(const char* moduleID, wasmtime_context_t* context)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        if (const auto it = threads_.find(ThreadInformation{moduleID, context}); it != threads_.end()) threads_.erase(it);
    }

    const WasmRuntimeModule* Get(const char* id)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        if (!id) return nullptr;

        auto it = modules_.find(id);
        if (it != modules_.end()) {
            return it->second;
        }
        return nullptr;
    }

    const WasmThreadInstance* GetThread(const char* moduleID, wasmtime_context_t* context)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        if (!context) return nullptr;
        auto it = std::ranges::find_if(threads_, [&](const auto& pair) {
            const auto& thread_ptr = pair.first;
            return thread_ptr.moduleID == moduleID && thread_ptr.context == context;
        });

        if (it != threads_.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool IsContextUsedInThread(wasmtime_context_t* context)
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        if (!context) return false;
        auto it = std::ranges::find_if(threads_, [&](const auto& pair) {
            const auto& thread_ptr = pair.first;
            return thread_ptr.context == context;
        });

        return it != threads_.end();
    }
};
