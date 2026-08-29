#pragma once
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include "hostSymbolTemplate.hpp"
#include "host/LinkerTemplate.h"

class BoraThreadSymbols : public HostSymbolTemplate {
public:
    const char* get_namespace() const override {
        return "bora::stl::thread";
    }
    static  std::atomic<int> id_counter;
    static std::map<int, std::thread> threads;
   static std::mutex mutex;

public:
    static int32_t create_thread(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 func_ptr, int32_t arg);
    static int32_t join_thread(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 thread_id);

    void bind_symbols(wasmtime_linker_t* linker) const override
    {
        const char* ns = get_namespace();
        LinkerTemplate::Function(linker, ns, "createThread", create_thread);
        LinkerTemplate::Function(linker, ns, "joinThread", join_thread);
    }
};

// Static members
