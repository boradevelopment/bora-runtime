#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include "hostSymbolTemplate.hpp"

class BoraThreadSymbols : public HostSymbolTemplate {
public:
    const char* get_namespace() const override {
        return "bora::stl::thread";
    }
    static  std::atomic<int> id_counter;
    static std::map<int, std::thread> threads;
   static std::mutex mutex;

public:
    static int32_t create_thread(wasm_exec_env_t exec_env, u64 func_ptr, int32_t arg);
    static int32_t join_thread(wasm_exec_env_t exec_env, u64 thread_id);
    std::vector<NativeSymbol> symbols = {
            {
                    "createThread",
                    (void*)create_thread,
                    "(Ii)i",
                    nullptr
            },
{"joinThread",   (void*)join_thread,   "(I)i",  nullptr}
    };

    const std::vector<NativeSymbol>& get_symbols() const override {
        return symbols;
    }
};

// Static members
