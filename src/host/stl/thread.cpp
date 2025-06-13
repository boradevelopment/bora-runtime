#include "thread.h"

std::atomic<int> BoraThreadSymbols::id_counter{1};

std::map<int, std::thread> BoraThreadSymbols::threads;
std::mutex BoraThreadSymbols::mutex;


int32_t BoraThreadSymbols::create_thread(wasm_exec_env_t exec_env, int32_t func_ptr, int32_t arg){
    int thread_id = id_counter++;


    if(!exec_env) return 0;

    std::thread t([=]() {
        if (!wasm_runtime_init_thread_env()) {
            fprintf(stderr, "Failed to init thread env in thread\n");
            return -1;
        }

        wasm_runtime_thread_env_inited();

        wasm_exec_env_t new_env = wasm_runtime_create_exec_env(
                wasm_runtime_get_module_inst(exec_env), 64 * 1024);

        if (!new_env) return -1;
        uint32_t argv[1] = { static_cast<uint32_t>(arg) };
       bool success = wasm_runtime_call_indirect(new_env, func_ptr, 1, argv);
        if (!success) {
            const char* error = wasm_runtime_get_exception(wasm_runtime_get_module_inst(exec_env));
            std::cerr << "WASM call failed: " << (error ? error : "unknown error") << std::endl;
        }
        wasm_runtime_destroy_exec_env(new_env);
    });

    {
        std::lock_guard<std::mutex> lock(mutex);
        threads[thread_id] = std::move(t);
        threads[thread_id].detach();
    }

    return thread_id;
}