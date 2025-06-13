// DEPRECATED - 6/13/2025

//#pragma once
//
//#include <string>
//#include <map>
//#include <mutex>
//#include <atomic>
//#include <vector>
//#include "hostSymbolTemplate.hpp"
//
//class BoraStringSymbols : public HostSymbolTemplate {
//public:
//    const char* get_namespace() const override {
//        return "bora::stl::string";
//    }
//
//    // ID counter for unique string handles
//    static std::atomic<int32_t> id_counter;
//    static std::map<int32_t, std::string> strings;
//    static std::mutex mutex;
//
//    // Create a new string (empty if ptr/len == 0)
//    static int32_t create_string(wasm_exec_env_t exec_env, int32_t ptr) {
//        std::lock_guard<std::mutex> lock(mutex);
//
//        std::string content;
//        if (ptr != 0) {
//            wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
//            const char* mem = reinterpret_cast<const char*>(wasm_runtime_addr_app_to_native(module_inst, ptr));
//            content.assign(mem);
//        }
//
//        int32_t id = id_counter++;
//        strings[id] = std::move(content);
//        return id;
//    }
//
//    // Return string length by ID
//    static int32_t get_length(wasm_exec_env_t, int32_t id) {
//        std::lock_guard<std::mutex> lock(mutex);
//        auto it = strings.find(id);
//        return (it != strings.end()) ? static_cast<int32_t>(it->second.size()) : -1;
//    }
//
//    // Return character at index, or -1 if invalid
//    static int32_t char_at(wasm_exec_env_t, int32_t id, int32_t index) {
//        std::lock_guard<std::mutex> lock(mutex);
//        auto it = strings.find(id);
//        if (it != strings.end() && index >= 0 && index < static_cast<int32_t>(it->second.size())) {
//            return static_cast<unsigned char>(it->second[index]);
//        }
//        return -1;
//    }
//
//    static int32_t c_str(wasm_exec_env_t exec_env, int32_t id){
//        std::lock_guard<std::mutex> lock(mutex);
//
//        auto it = strings.find(id);
//        if (it == strings.end()) return 0;
//
//        const std::string& str = it->second;
//        uint32_t len = static_cast<uint32_t>(str.size()) + 1;
//
//        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
//        if (!module_inst) {
//            printf("[c_str] Error: module_inst is null\n");
//            return 0;
//        }
//
//        void* native_ptr = nullptr;
//        uint32_t offset = wasm_runtime_module_malloc(module_inst, len, &native_ptr);
//        if (offset == 0 || !native_ptr) {
//            printf("[c_str] Error: malloc of %u bytes failed\n", len);
//            return 0;
//        }
//
//        memcpy(native_ptr, str.c_str(), len);  // Copy null-terminated string
//        return static_cast<int32_t>(offset);
//    }
//
//    static void free_cstring(wasm_exec_env_t exec_env, int32_t offset) {
//        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
//        if (module_inst && offset) {
//            wasm_runtime_module_free(module_inst, offset);
//        }
//    }
//
//    static void free_string(wasm_exec_env_t exec_env, int32_t id) {
//        // Obtain module instance (not used in this case, but may be useful later)
//        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
//
//        // Lock the string map to safely remove the string
//        std::lock_guard<std::mutex> lock(mutex);
//        strings.erase(id); // Just remove the entry if it exists
//    }
//
//
//
//    // Append string content from memory
//    static void append_string(wasm_exec_env_t exec_env, int32_t id, int32_t ptr) {
//        std::lock_guard<std::mutex> lock(mutex);
//        auto it = strings.find(id);
//
//        if (it != strings.end() && ptr != 0) {
//            wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
//            const char* mem = reinterpret_cast<const char*>(wasm_runtime_addr_app_to_native(module_inst, ptr));
//            it->second.append(mem);
//        }
//    }
//
//    // Copy string content to WASM memory (caller allocates buffer)
//    static void get_string(wasm_exec_env_t exec_env, int32_t id, int32_t ptr) {
//        std::lock_guard<std::mutex> lock(mutex);
//        auto it = strings.find(id);
//        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
//        if (it != strings.end()) {
//            char* dst = reinterpret_cast<char*>(wasm_runtime_addr_app_to_native(module_inst, ptr));
//            std::memcpy(dst, it->second.data(), it->second.size());
//        }
//    }
//
//
//
//
//    // Native symbols exposed to WASM
//    std::vector<NativeSymbol> symbols = {
//            { "create",  (void*)create_string, "(i)i", nullptr },
//            { "length",  (void*)get_length,    "(i)i",  nullptr },
//            { "charAt",  (void*)char_at,       "(ii)i", nullptr },
//            { "append",  (void*)append_string, "(ii)", nullptr },
//            { "get",     (void*)get_string,    "(ii)",  nullptr },
//            { "c_str", (void*)c_str, "(i)i", nullptr },
//            { "free_cstr", (void*)free_cstring, "(i)", nullptr },
//            { "free", (void*)free_string, "(i)", nullptr }
//
//    };
//
//    const std::vector<NativeSymbol>& get_symbols() const override {
//        return symbols;
//    }
//};
//
//// Static definitions
