#include "thread.h"

std::atomic<int> BoraThreadSymbols::id_counter{1};

std::map<int, std::thread> BoraThreadSymbols::threads;
std::mutex BoraThreadSymbols::mutex;


int32_t BoraThreadSymbols::create_thread(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 func_ptr, int32_t arg){
   // unfinished
    return 0;
}

int32_t BoraThreadSymbols::join_thread(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 thread_id) {
    std::thread t;

    // 1. Find and extract the thread from the map
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = threads.find(thread_id);
        if (it == threads.end()) return -1; // Thread not found or already joined

        t = std::move(it->second);
        threads.erase(it);
    }

    // 2. Wait for the physical thread to finish
    if (t.joinable()) {
        t.join();
    }

    return 0; // Success
}
