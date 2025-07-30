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
        enum class MessageBoxType : int64_t {
            NONE = 0,
            INFORMATION = 1,
            WARNING = 2,
            PROBLEM = 3
        };


    struct messageBoxConfig {
        int64_t title_ptr;        // Pointer to title string
        int64_t message_ptr;      // Pointer to message string

        MessageBoxType type;      // Icon/severity style
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

        // Copy struct from WASM memory to native struct
        messageBoxConfig config;
        std::memcpy(&config, native_data, sizeof(config));

        // Resolve title and message strings
        const char* title = reinterpret_cast<const char*>(
                wasm_runtime_addr_app_to_native(inst, (uint32_t)config.title_ptr)
        );
        const char* msg = reinterpret_cast<const char*>(
                wasm_runtime_addr_app_to_native(inst, (uint32_t)config.message_ptr)
        );

        std::string title_str = title ? title : "(null)";
        std::string message_str = msg ? msg : "(null)";

        std::cout << "[MessageBox]\nTitle: " << title_str
                  << "\nMessage: " << message_str
                  << "\nType: " << static_cast<int>(config.type)
                  << std::endl;


#if WIN32
        // Show native Windows message box (example: ERROR icon, OK only)
        UINT iconType = MB_OK;
        switch (config.type) {
            case MessageBoxType::INFORMATION: iconType = MB_ICONINFORMATION | MB_OK; break;
            case MessageBoxType::WARNING:     iconType = MB_ICONWARNING | MB_OK; break;
            case MessageBoxType::PROBLEM:     iconType = MB_ICONERROR | MB_OK; break;
            default:                          iconType = MB_OK; break;
        }

        MessageBoxA(NULL, message_str.c_str(), title_str.c_str(), iconType);
#else
        printf("NOT SUPPORTED\n");
#endif

        return 1;
    }


private:
    std::vector<NativeSymbol> symbols = {
            { "cout",    (void*)print,       "(I)i", nullptr },
            { "cerr",   (void*)printError,  "(I)i", nullptr },
            { "msgbox", (void*)messageBox,  "(I)i", nullptr },
    };

public:
    const std::vector<NativeSymbol>& get_symbols() const override {
        return symbols;
    }
};
