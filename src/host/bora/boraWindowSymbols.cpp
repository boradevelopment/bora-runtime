// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

#include "boraWindowSymbols.h"

#include "Utilities.h"
#include "host/Commands.h"
#include "host/WasmTools.h"
#include "nWindow/bnUserWindow.h"


struct HostCommandListRecord;

typedef void (*UpdateProcWASM)(const char* wndID, void* userObject, unsigned int msg, u64 wParam, intptr_t lParam);

struct WASMWindowConfig {
    const char* id = "Window";
    UpdateProcWASM update;
};

void BoraWindowSymbols::windowCommandHookInit() {
    HostCommandManager::AddOnInitializeHook([](u64 handle, const HostCommandListRecord& record) {
        if (record.category != CommandCategories::NativeGraphics) return;
        u64 windowID = std::stoull(record.name);
});
}

u64 BoraWindowSymbols::createWindow(wasm_exec_env_t exec_env, u64 configuration_ptr, u64 update_ptr) {
    const char* id = WasmTools::fromWASM<const char*>(exec_env, configuration_ptr);
    const void* updatePointer = WasmTools::fromWASM<const void*>(exec_env, update_ptr);
    bnUserWindowConfig e;
    e.id = utf8ToWstring(id);
    e.title = utf8ToWstring(id);
    windows[windows.size()+1] = new bnUserWindow(id, updatePointer, e);
    return windows.size();
}

u64 BoraWindowSymbols::runWindow(wasm_exec_env_t exec_env, u64 window_ptr) {
    return 0;
}

u64 BoraWindowSymbols::closeWindow(wasm_exec_env_t exec_env, u64 window_ptr) {
    return 0;
}
