#pragma once

#include "hostSymbolTemplate.hpp"
#include "host/LinkerTemplate.h"
#include "nWindow/bnUserWindow.h"

class BoraWindowSymbols : public HostSymbolTemplate {
public:
 static inline std::unordered_map<int, bnUserWindow*> windows;
 const char *get_namespace() const override {
  return "bora::window";
 }
 static void windowCommandHookInit();
 static u64 createWindow(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 configuration_ptr, u64 idk);
 static u64 runWindow(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 window_ptr);
 static u64 closeWindow(wasmtime_caller_t* moduleCaller, wasmtime_context_t* moduleContext, u64 window_ptr);

 void bind_symbols(wasmtime_linker_t* linker) const override
 {
  const char* ns = get_namespace();
  LinkerTemplate::Function(linker, ns, "create", createWindow);
  LinkerTemplate::Function(linker, ns, "run", runWindow);
  LinkerTemplate::Function(linker, ns, "close", closeWindow);
 }
};
