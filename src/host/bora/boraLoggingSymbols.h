#include "hostSymbolTemplate.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstring> // for memcpy
#if WIN32
#include "Windows.h"
#if defined _WIN64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "Comctl32.lib")
#endif

class BoraLoggingSymbols : public HostSymbolTemplate {
public:
    const char* get_namespace() const override {
        return "bora::logging";
    }

    // Struct passed from WASM
    struct messageBoxConfig {
        int32_t title_ptr;
        int32_t message_ptr;
        // Extend this with buttons, flags, etc.
    };

    static int32_t print(wasm_exec_env_t exec_env, int32_t app_ptr) {
        const char* str = reinterpret_cast<const char*>(
                wasm_runtime_addr_app_to_native(wasm_runtime_get_module_inst(exec_env), app_ptr)
        );
        std::cout << "[Print] " << std::string(str) << std::endl;
        return 0;
    }

    static int32_t printError(wasm_exec_env_t exec_env, int32_t app_ptr) {
        const char* str = reinterpret_cast<const char*>(
                wasm_runtime_addr_app_to_native(wasm_runtime_get_module_inst(exec_env), app_ptr)
        );
        std::cerr << "[Error] " << std::string(str) << std::endl;
        return 0;
    }

    static int32_t messageBox(wasm_exec_env_t exec_env, int32_t app_ptr, int32_t len) {
        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
        if (!inst) return -1;

        void* native_data = wasm_runtime_addr_app_to_native(inst, app_ptr);
        if (!native_data) return -2;

        messageBoxConfig config;
        std::memcpy(&config, native_data, sizeof(config));

        const char* title = reinterpret_cast<const char*>(
                wasm_runtime_addr_app_to_native(inst, config.title_ptr)
        );
        const char* msg = reinterpret_cast<const char*>(
                wasm_runtime_addr_app_to_native(inst, config.message_ptr)
        );

        std::string title_str(title);
        std::string message_str(msg);

        std::cout << "[MessageBox]\nTitle: " << title_str << "\nMessage: " << message_str << std::endl;


        MessageBoxA(NULL, title_str.c_str(), message_str.c_str(), MB_ICONERROR);


        return 1;
    }

private:
    std::vector<NativeSymbol> symbols = {
            { "cout",    (void*)print,       "(i)i", nullptr },
            { "cerr",   (void*)printError,  "(i)i", nullptr },
            { "msgbox", (void*)messageBox,  "(i)i", nullptr },
    };

public:
    const std::vector<NativeSymbol>& get_symbols() const override {
        return symbols;
    }
};
