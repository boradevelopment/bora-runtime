#include "hostSymbolTemplate.hpp"

class BoraMemorySymbols : public HostSymbolTemplate {
public:
    const char *get_namespace() const override {
        return "bora::memory";
    }

    static int tSize;

    static uint64_t host_alloc_pointer(wasm_exec_env_t exec_env, uint64_t size, uint64_t ptr) {
        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);

        if (!module_inst) {
            printf("[allocate] Error: module_inst is null\n");
            return 1; // error

        }

        uint64_t allocated_offset = wasm_runtime_module_malloc(module_inst, size, nullptr);
        if (allocated_offset == 0) {
            return 1; // allocation failed
        }

        // Now, write allocated_offset back *into WASM memory* at wasm_ptr_to_ptr_offset
        // Convert WASM memory offset to native pointer:
        auto* wasm_memory = static_cast<uint8_t *>(wasm_runtime_addr_app_to_native(module_inst, ptr));
        if (!wasm_memory) {
            printf("[allocate] Error: cannot get native pointer for wasm_ptr_to_ptr_offset\n");
            return 1;
        }

        // Store the 64-bit offset (allocated pointer) at wasm_ptr_to_ptr_offset in WASM memory
        *(uint64_t*)wasm_memory = allocated_offset;

        return 0; // success
    }

    static uint64_t host_alloc(wasm_exec_env_t exec_env, uint64_t size) {
        // Allocate memory using the WASM runtime allocator

        tSize += size;
        std::cout << "new mem allocated :" << tSize << std::endl;

        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
        if (!module_inst) {
            printf("[allocate] Error: module_inst is null\n");
            return 0;
        }

        uint64_t offset = wasm_runtime_module_malloc(module_inst, size, nullptr);

        return offset;
    }

    static void host_free(wasm_exec_env_t exec_env, uint64_t offset) {
        tSize -= sizeof(uint64_t);
        std::cout << "new mem deallocated :" << tSize << std::endl;

        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
        if (module_inst && offset) {
            wasm_runtime_module_free(module_inst, offset);
        }
    }

    std::vector<NativeSymbol> symbols = {
            {
                    "alloc",
                    (void*)host_alloc,
                    "(I)I",
                    nullptr
            },
            {
                "alloc_ptr",
                (void*) host_alloc_pointer,
                "(II)I",
                nullptr
            },
            {
                    "free",
                    (void*)host_free,
                    "(I)",
                    nullptr
            }
    };

    const std::vector<NativeSymbol>& get_symbols() const override {
        return symbols;
    }
};
