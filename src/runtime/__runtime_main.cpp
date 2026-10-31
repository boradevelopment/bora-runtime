// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
// Main Function for a regular runtime process
#include "__runtime_main.h"
#include "TAZA.h"
#include <stdlib.h>
#include <string.h>
#include "wasmtime.hh"
#include "guest/wasm/WasmRunner.h"
#include "guest/bora/Commands.h"
#include "guest/stl/thread.h"
#include "logging/LogManager.h"
#include "host/common/nWindow/bnMessageBox.h"
#include "host/common/nWindow/bnUserWindow.h"
#include "tools/AppParam.h"
#include "tools/CPUInfo.h"
#include "guest/wasm/WasmGlobal.h"
#include "nNetworking/DomainSocketFactory.h"
#include "nNetworking/IDomainSocket.h"

#if __linux__
#include <dlfcn.h>
#endif

// gdb debugging does not return WASI code properly...
const int app_instance_main(wasmtime_context_t* context, const WasmRuntimeModule* module_inst)
{
    wasmtime_extern_t item;
    bool found = wasmtime_instance_export_get(context, &module_inst->mainThread.instance, "_start", 6, &item);

    if (!found || item.kind != WASMTIME_EXTERN_FUNC) {
        found = wasmtime_instance_export_get(context, &module_inst->mainThread.instance, "main", 4, &item);
    }
    if (!found || item.kind != WASMTIME_EXTERN_FUNC) {
        std::cerr << "Could not find _start or main function inside instance!\n";
        return 2;
    }

    wasmtime_func_t entry_func = item.of.func;
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *error = nullptr;
    if (WasmGlobal::getInstance()->getDebugging()) error = bwasmtime_gdb_call_func(WasmGlobal::getInstance()->getDebugServer(), WasmGlobal::getInstance()->getStore(), &entry_func, nullptr, 0, nullptr, nullptr, 0);
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

            wasmtime_global_get(context, &module_inst->mainThread.globalMemoryBase, &mem_base_ext);
            int64_t mem_base = mem_base_ext.of.i64;
            std::cerr << "   [Debug State] __memory_base = " << mem_base << "\n";

            wasmtime_global_get(context, &module_inst->mainThread.globalStack, &stack_ptr_ext);
            int64_t stackPtr = stack_ptr_ext.of.i64;
            std::cerr << "   [Debug State] stackPtr = " << stackPtr << "\n";

            wasm_name_delete(&msg);
            wasm_trap_delete(trap);
            return 3;
        }

    return 3;
}

#include "host/common/nWindow/bnWindow.h"
#include "host/common/nWindow/devBnLogoWindow.h"
#include "host/common/nWindow/bnWindowTitlebar.h"
static void updateFunction(bnUserWindow* window, void* userObject, unsigned int msg, uintptr_t wParam, intptr_t lParam) {
if (msg == ON_UPDATE){
    // std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
}

bnWindow* win;
// ULONG_PTR gdiplusToken;


int
runtime_main(int argc, char *argv[]){
    auto rootLogger = LogManager::instance().getLogger("root");
    AppParam::registerParam("debug", {"-db"});
    AppParam::initialize(argc, argv);
    if (AppParam::has("verbosity"))
    {
        rootLogger->setVerbosityLimit(AppParam::getValue<int>("verbosity"));
    }


    // auto client = DomainSocketFactory::createClient("/tmp/bora_supervisor.sock");
    // if (!client->isConnected())
    // {
    //     LOG_ERROR(rootLogger) << "Could not connect to supervisor.";
    //     return 1;
    // }
    // while (client->isConnected()) {
    //     client->send("Hello Server", 12);
    // }

    // GdiplusStartupInput gdiplusStartupInput;
    // GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
     // CreateBlockingMessageBox(L"test", L"Messagebox Title", L"Messagebox Body for descriptions and details");
     //std::cin.get();
     // devBnLogoWindow();
    //  std::cin.get();
    // auto e = new bnWindowTitlebarConfig();
    // e->buttonHoverColor = { 25, 25, 25 };
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
    // win->run();
    // delete win;
    //
    // return 0;
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
    char *wasm_file = argv[1];
    if (wasm_file == nullptr)
    {
        // open settings but will just say nothing for now.
        printf("Provide a bora application\n");
        return 0;
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

    wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_SPEED);
    wasmtime_config_wasm_simd_set(config, true);
    wasmtime_config_wasm_memory64_set(config, true);
    wasmtime_config_wasm_threads_set(config, true);
    wasmtime_config_shared_memory_set(config, true);
    wasmtime_config_async_stack_size_set(config, 1 * 1024 * 1024);
    wasmtime_config_max_wasm_stack_set(config, 1 * 1024 * 1024);
    wasmtime_config_epoch_interruption_set(config, true);
    wasmtime_config_wasm_component_model_async_set(config, true);
    // wasmtime_config_memory_reservation_set(config, 16ULL * 1024 * 1024 * 1024);
    wasm_engine_t *engine = nullptr;
    WasmGlobal linker;
    if (AppParam::has("debug"))
    {
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

    wasmtime_store_t *store = wasmtime_store_new(engine, nullptr, nullptr);
    if (!store) {
        fprintf(stderr, "Failed to create Wasmtime store.\n");
        wasm_engine_delete(engine);
        return -1;
    }
    wasmtime_store_epoch_deadline_callback(store, BoraThreadSymbols::stub_epoch_callback, nullptr, nullptr); // stub
    wasmtime_context_t *context = wasmtime_store_context(store);

    linker = std::move(WasmGlobal(context, engine, store));
    if (AppParam::has("debug"))
    {

        // wasmtime_context_set_epoch_deadline(context, -1);
        wasmtime_context_epoch_deadline_async_yield_and_update(context, 1);
    }
    wasi_config_t *wasi_config = wasi_config_new();
    wasi_config_inherit_stdout(wasi_config);
    wasi_config_inherit_stderr(wasi_config);
    wasi_config_inherit_stdin(wasi_config);
    wasi_config_preopen_dir(wasi_config, "./", "./", WASMTIME_WASI_DIR_PERMS_READ | WASMTIME_WASI_DIR_PERMS_WRITE, WASMTIME_WASI_FILE_PERMS_READ | WASMTIME_WASI_FILE_PERMS_WRITE);
    wasmtime_store_limiter(store, INT64_MAX, -1, -1, -1, -1);

    const WasmRuntimeModule* mainModule = WasmRunner::loadModuleByTAZA(&linker, store, context, engine, wasm_file);
    if (mainModule == nullptr)
    {
        linker.destroy();
        printf("Unable to execute file as it is not a valid BORA application.");
        wasm_engine_delete(engine);
        return 1;
    }
    wasmtime_error_t *error = wasmtime_context_set_wasi(context, wasi_config);
    if (error != nullptr) {
        std::cerr << "Failed to set WASI configuration.\n";
        linker.destroy();
        wasmtime_error_delete(error);
        return -1;
    }
    BoraThreadSymbols::CreateEpochThread();
    HostCommandManager::AddOnUpdateHook([](u64 handle, const HostCommandListRecord& record) {
        CommandRegistry::ExecuteCommandBuffer(record.instance, record.module, nullptr, (u8*)WasmTools::fromWASM<u8*>(record.instance, record.module, record.wasmPtr), record.size);
    });

    wasmtime_func_t func;
    if (!WasmTools::getFunctionInInstance(&mainModule->mainThread, "get_bora_sdk_version", &func)) {
        printf("Unable to get SDK version function! This is not a proper BORA Universal Assembly");
        return 0;
    } else
    {
        auto versionPointer = WasmTools::RequestExportedMethod<u32*>(&mainModule->mainThread, mainModule, "get_bora_sdk_version");
        auto version = WasmTools::fromWASM<const char*>(&mainModule->mainThread, mainModule, (u64)versionPointer);
        LOG_INFO_VERBOSE(LogManager::instance().getLogger("bora"), 1) << "Module is running BORA SDK: " << version;
    }

    int ret = app_instance_main(context, mainModule);
    if (error) wasmtime_error_delete(error);
    BoraThreadSymbols::ShutdownAllThreads();
    linker.destroy(); // dependencies need to be destroyed before mainModule.
    mainModule->destroy();
    wasm_engine_delete(engine);
    return ret;
}
