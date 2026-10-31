#pragma once
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include "guest/templates/hostSymbolTemplate.hpp"
#include "../templates/LinkerTemplate.h"

enum class BoraThreadState {
    Running,     // Actively executing Wasm instructions
    Interrupted, // Paused/interrupted safely
    Detached,    // Detached from tracking; will self-destruct on exit
    Finished     // Execution completed, waiting to be joined or cleaned up
};

struct BoraThreadInterruptState {
    const WasmThreadInstance* instance = nullptr;
    std::mutex mtx;
    std::condition_variable cv;
    std::condition_variable sleepCV;
    std::atomic<bool> paused = false;      // True if we want to pause/interrupt
    std::atomic<bool> should_stop = false; // True if we want to kill the thread entirely
};


struct BoraModuleThread
{
    BoraModuleThread(i64 id, const WasmRuntimeModule* module, const WasmThreadInstance* instance) : id(id), module(module), instance(instance)
    {
        interrupt_state.instance = instance;
    }
    const WasmRuntimeModule* module;
    const WasmThreadInstance* instance;
    i64 id;
    std::thread thread;
    u64 func_ptr{}, arg{};

    std::atomic<BoraThreadState> state;
    BoraThreadInterruptState interrupt_state;
    bool isMain = false;
    const char* name{};

    ~BoraModuleThread()
    {
        if (thread.joinable()) {
            thread.join();
        }
    }
};

class BoraThreadSymbols : public HostSymbolTemplate {
public:
    const char* get_namespace() const override {
        return "bora::thread";
    }

    static inline sUnorderedMap<int, sUniquePointer<BoraModuleThread>> threads;
    static inline std::atomic<int> id_counter = -1;
    static inline std::atomic<bool> epochThreadActive;
    static inline std::thread epochThread;
    static inline std::mutex mutex;

public:
    static void CreateMainThread(const WasmRuntimeModule* module);
    static void CreateEpochThread();
    static void ShutdownAllThreads();

    static int create_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller,  u64 func_ptr, u64 arg, u64 namePtr);
    static int join_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, int thread_id);
    static void interrupt_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, int thread_id);
    static void resume_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, int thread_id);
    static void kill_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, int thread_id);
    static void detach_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, int thread_id);
    static void set_thread_name(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, int thread_id, u64 namePtr);
    static u64 get_thread_name(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, int thread_id);
        static i64 get_thread_id(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller);
    static void thread_sleep_for(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, u64 time);
    static void thread_sleep_until(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller, u64 time);
    static void thread_sleep_yield(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller);

    static wasmtime_error_t* stub_epoch_callback(
        wasmtime_context_t* context,
        void* data,
        uint64_t* epoch_deadline_delta,
        wasmtime_update_deadline_kind_t* update_kind
    )
    {
        return nullptr;
    }

    void bind_symbols(wasmtime_linker_t* linker) const override
    {
        const char* ns = get_namespace();
        LinkerTemplate::Function(linker, ns, "createThread", create_thread);
        LinkerTemplate::Function(linker, ns, "joinThread", join_thread);
        LinkerTemplate::Function(linker, ns, "interruptThread", interrupt_thread);
        LinkerTemplate::Function(linker, ns, "resumeThread", resume_thread);
        LinkerTemplate::Function(linker, ns, "killThread", kill_thread);
        LinkerTemplate::Function(linker, ns, "detachThread", detach_thread);
        LinkerTemplate::Function(linker, ns, "setThreadName", set_thread_name);
        LinkerTemplate::Function(linker, ns, "getThreadName", get_thread_name);
        LinkerTemplate::Function(linker, ns, "getThreadId", get_thread_id);
        LinkerTemplate::Function(linker, ns, "sleepFor", thread_sleep_for);
        LinkerTemplate::Function(linker, ns, "sleepUntil", thread_sleep_until);
        LinkerTemplate::Function(linker, ns, "yield", thread_sleep_yield);
    }
};

// Static members
