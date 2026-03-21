#include "hostSymbolTemplate.hpp"
#include "nWindow/bnUserWindow.h"

class BoraWindowSymbols : public HostSymbolTemplate {
public:
 static inline std::unordered_map<int, bnUserWindow*> windows;
 const char *get_namespace() const override {
  return "bora::window";
 }
 static void windowCommandHookInit();
 static u64 createWindow(wasm_exec_env_t exec_env, u64 configuration_ptr, u64 idk);
 static u64 runWindow(wasm_exec_env_t exec_env, u64 window_ptr);
 static u64 closeWindow(wasm_exec_env_t exec_env, u64 window_ptr);
 std::vector<NativeSymbol> symbols = {
  {
   "create",
   (void*)createWindow,
   "(II)I",
   nullptr
},
{
 "run",
 (void*)runWindow,
 "(I)",
 nullptr
},
{
 "close",
 (void*)closeWindow,
 "(I)",
 nullptr
}
 };

 const std::vector<NativeSymbol>& get_symbols() const override {
  return symbols;
 }
};
