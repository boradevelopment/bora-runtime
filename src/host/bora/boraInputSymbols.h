#include "hostSymbolTemplate.hpp"

class BoraInputSymbols : public HostSymbolTemplate {
public:
    const char *get_namespace() const override {
        return "bora::input";
    }
    static int32_t host_cin(wasm_exec_env_t exec_env, int32_t app_ptr, int32_t len);
    std::vector<NativeSymbol> symbols = {
            {
                    "cin",
                    (void*)host_cin,
                    "(ii)i",
                    nullptr
            }
    };

    const std::vector<NativeSymbol>& get_symbols() const override {
        return symbols;
    }
};