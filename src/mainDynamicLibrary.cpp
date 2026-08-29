// this is a huge pile of dogshit, clean up this up immediately soon.
#pragma once
#ifndef WRAPPER
#include "TAZA.h"
#include <stdlib.h>
#include <string.h>
#include "wasmtime.hh"
#include "WasmRunner.h"
#include "host/Commands.h"
#include "logging/LogManager.h"
#include "nWindow/bnMessageBox.h"
#include "nWindow/bnUserWindow.h"
#include "tools/AppParam.h"
#include "tools/CPUInfo.h"
#include "wasm/WasmLibraryLinker.h"

#if __linux__
#include <dlfcn.h>
#endif


static int app_argc;
static char **app_argv;

#define MODULE_PATH ("--module-path=")

using InitializeSymbolsFunc = int(*)(int, char * b[]);

#include <cstring>
#include <string>
#include <iostream>
#include <vector>
#include "SysImageMgr.h"
#include "software/win32/rcscle.h"


bool IsRunningAsDLL() {
#if WIN32
    // Get handle of current module (DLL or EXE)
    HMODULE hModule = nullptr;
    // If compiled inside DLL, GetModuleHandleEx can be used with address inside module
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)&IsRunningAsDLL, &hModule)) {
        // Get the module file name
        TCHAR path[MAX_PATH];
        if (GetModuleFileName(hModule, path, MAX_PATH)) {
            // Check extension for .dll or .exe (simple)
            std::wstring filename(path);
            size_t pos = filename.rfind('.');
            if (pos != std::string::npos) {
                std::wstring ext = filename.substr(pos);
                if (wcscmp(ext.c_str(), L".dll") == 0) {
                    return true;  // Running inside DLL
                }
            }
        }
    }
    return false;  // Probably EXE
#elif __linux__
    Dl_info info = {};
    // Take the address of some function or symbol in your code
    if (dladdr((void*)IsRunningAsDLL, &info) != 0 && info.dli_fname) {
        std::string filename(info.dli_fname);
        // Check extension for .so (shared object)
        size_t pos = filename.rfind('.');
        if (pos != std::string::npos) {
            std::string ext = filename.substr(pos);
            if (ext == ".so" || ext.find(".so.") != std::string::npos) {
                return true;  // Running inside shared library
            }
        }
    }
    return false; // Probably executable
#endif
}

// gdb debugging does not return WASI code properly...
const int app_instance_main(wasmtime_context_t* context, const WasmRuntimeModule* module_inst)
{
    WasmTools::setLastContext(context);
    wasmtime_extern_t item;
    bool found = wasmtime_instance_export_get(context, &module_inst->instance, "_start", 6, &item);

    if (!found || item.kind != WASMTIME_EXTERN_FUNC) {
        found = wasmtime_instance_export_get(context, &module_inst->instance, "main", 4, &item);
    }
    if (!found || item.kind != WASMTIME_EXTERN_FUNC) {
        std::cerr << "Could not find _start or main function inside instance!\n";
        return 2;
    }


    wasmtime_func_t entry_func = item.of.func;
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *error = nullptr;
    if (WasmLibraryLinker::getInstance()->getDebugging()) error = bwasmtime_gdb_call_func(WasmLibraryLinker::getInstance()->getDebugServer(), WasmLibraryLinker::getInstance()->getStore(), &entry_func, nullptr, 0, nullptr, nullptr, 0);
    else error = wasmtime_func_call(context, &entry_func, nullptr, 0, nullptr, 0, &trap);

    if (error == nullptr && trap == nullptr) {
        printf("Guest returned: 0\n");
        return 0;
    }

    if (error != nullptr) {
        int exit_code = 0;
        // Wasmtime packages WASI exit calls into standard engine errors
        if (wasmtime_error_exit_status(error, &exit_code)) {
            wasmtime_error_delete(error);
            return exit_code;
        }

        // It wasn't a standard exit; it was a physical engine execution failure
        wasm_name_t msg;
        wasmtime_error_message(error, &msg);
        std::cerr << "Wasmtime execution engine error: " << std::string(msg.data, msg.size) << "\n";
        wasm_name_delete(&msg);
        wasmtime_error_delete(error);
        return 3;
    }

        if (trap != nullptr) {
            wasm_name_t msg;
            wasm_trap_message(trap, &msg);
            std::cerr << "Wasm execution trapped! Call Stack:\n" << std::string(msg.data, msg.size) << "\n";

            // 1. Log Memory Base & Table Base for dynamic libraries
            wasmtime_val_t mem_base_ext;
            wasmtime_val_t stack_ptr_ext;

            wasmtime_global_get(context, &module_inst->globalMemoryBase, &mem_base_ext);
            int64_t mem_base = mem_base_ext.of.i64;
            std::cerr << "   [Debug State] __memory_base = " << mem_base << "\n";

            wasmtime_global_get(context, &module_inst->globalStack, &stack_ptr_ext);
            int64_t stackPtr = stack_ptr_ext.of.i64;
            std::cerr << "   [Debug State] stackPtr = " << stackPtr << "\n";

            wasm_name_delete(&msg);
            wasm_trap_delete(trap);
            return 3;
        }

    return 3;
}

#include "software/common/nWindow/bnWindow.h"
#include "software/common/nWindow/devBnLogoWindow.h"
#include "software/common/nWindow/bnWindowTitlebar.h"

static void updateFunction(bnUserWindow* window, void* userObject, unsigned int msg, uintptr_t wParam, intptr_t lParam) {
if (msg == ON_UPDATE){
    // std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
}

bnWindow* win;
// ULONG_PTR gdiplusToken;



int
main(int argc, char *argv[]){
    // GdiplusStartupInput gdiplusStartupInput;
    // GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
     // CreateBlockingMessageBox(L"test", L"Messagebox Title", L"Messagebox Body for descriptions and details");
     //std::cin.get();
     // devBnLogoWindow();
    //  std::cin.get();
    // auto e = new bnWindowTitlebarConfig();
    // // e->buttonHoverColor = { 25, 25, 25 };
    // e->borderColor = { 255, 255, 255 };
    // e->sysButtons = 0;
    // e->enabled = true;
    //
    // win = new bnUserWindow({ 1920, 1080, nullptr, L"BoraError1", L"Wano's application", -1, new Data(R"(./borat.png)"),
    // {
    //
    //     0, 0, 0,1,
    //
    // }, {0,0,0}, 8, true, e, updateFunction });
    //
    // win->run();
    // std::cin.get();
    // delete win;
    // win = nullptr;
    // GdiplusShutdown(gdiplusToken);
    // std::cin.get();
    // return 0;

    // If we want to output all logs to a file, we add the FileAppender to the root instance to the filepath the user wants.
    // Other children can use the same AppParam style check to create unique log files.
    auto rootLogger = LogManager::instance().getLogger("root");
    AppParam::registerParam("debug", {"-db"});
    AppParam::initialize(argc, argv);
#ifndef  NDEBUG // debug builds have a verbosity of 3 by default
    rootLogger->setVerbosityLimit(3);
#endif
    if (AppParam::has("verbosity"))
    {
        rootLogger->setVerbosityLimit(AppParam::getValue<int>("verbosity"));
    }





    // if (AppParam::has("logfile"))
    // { // todo: get string, convert to a fs path and add root to fileAppender!!!
    //     rootLogger->addAppender(std::make_shared<FileAppender>("./main_log.log"));
    // }



    wasm_config_t *config = wasm_config_new();
    if (!config) {
        fprintf(stderr, "Failed to create Wasmtime configuration.\n");
        return -1;
    }

    wasmtime_config_wasm_memory64_set(config, true);

    wasm_engine_t *engine = nullptr;
    WasmLibraryLinker linker;
    if (AppParam::has("debug"))
    {
        wasmtime_config_epoch_interruption_set(config, true);
        wasmtime_config_guest_debug_set(config, true);
        wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_NONE);
        wasmtime_config_debug_info_set(config, true);
        wasmtime_config_native_unwind_info_set(config, true);
        linker.enableDebugging(config, "127.0.0.1:12345");
        engine = linker.getEngine();
    } else
    {
        engine = wasm_engine_new_with_config(config);
        if (!engine) {
            fprintf(stderr, "Failed to initialize Wasmtime engine.\n");
            return -1;
        }
    }

    wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
    if (!store) {
        fprintf(stderr, "Failed to create Wasmtime store.\n");
        wasm_engine_delete(engine);
        return -1;
    }
    wasmtime_context_t *context = wasmtime_store_context(store);

    linker = std::move(WasmLibraryLinker(context, engine, store));
    if (AppParam::has("debug"))
    {

        // wasmtime_context_set_epoch_deadline(context, -1);
        wasmtime_context_epoch_deadline_async_yield_and_update(context, 1);
    }
    wasi_config_t *wasi_config = wasi_config_new();
    wasi_config_inherit_stdout(wasi_config);
    wasi_config_inherit_stderr(wasi_config);
    wasi_config_inherit_stdin(wasi_config);
    wasi_config_preopen_dir(wasi_config, ".", ".", WASMTIME_WASI_DIR_PERMS_READ | WASMTIME_WASI_DIR_PERMS_WRITE, WASMTIME_WASI_FILE_PERMS_READ | WASMTIME_WASI_FILE_PERMS_WRITE);

    wasmtime_error_t *error = wasmtime_context_set_wasi(context, wasi_config);
    if (error != nullptr) {
        std::cerr << "Failed to set WASI configuration.\n";
        wasmtime_error_delete(error);
        wasmtime_store_delete(store);
        return -1;
    }


    wasmtime_store_limiter(store, INT64_MAX, -1, -1, -1, -1);
    char *wasm_file = argv[1];
    const WasmRuntimeModule* mainModule = WasmRunner::loadModuleByTAZA(&linker, context, engine, wasm_file);
    if (mainModule == nullptr)
    {
        printf("Unable to execute file as it is not a valid BORA application.");
        wasm_engine_delete(engine);
        wasmtime_store_delete(store);
        return 1;
    }
    printf("I got the goods\n");

    HostCommandManager::AddOnUpdateHook([context, mainModule](u64 handle, const HostCommandListRecord& record) {
        std::cout << "UPDATE HOOK DETECTED :" << handle << std::endl;
    CommandRegistry::ExecuteCommandBuffer(context, mainModule, nullptr, (u8*)WasmTools::fromWASM<u8*>(context, mainModule, record.wasmPtr), record.size);
});

    wasmtime_func_t func;
    if (!WasmTools::getFunctionInInstance(context, mainModule, "get_bora_sdk_version", &func)) {
        printf("Unable to get SDK version function! This is not a proper BORA Universal Assembly");
        return 0;
    } else
    {
        auto versionPointer = WasmTools::RequestExportedMethod<u32*>(context, mainModule, "get_bora_sdk_version");
        auto version = WasmTools::fromWASM<const char*>(context, mainModule, (u64)versionPointer);
        printf("Running BORA SDK Version: %s\n", version);
    }

// #ifdef WASM_ENABLE_DEBUG_INTERP
//     if(AppParam::has("debug")) {
//         uint32_t debug_port = wasm_runtime_start_debug_instance(exec_env);
//         printf("Debugging at %d\n", debug_port);
//     }
// #endif



    int ret = 0;

    ret = app_instance_main(context, mainModule);
    if (ret == 3) return 3; // trap/exception - unsafe.
    mainModule->destroy();
    linker.destroy();
    if (error) wasmtime_error_delete(error);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);

    return ret;
}
#endif

