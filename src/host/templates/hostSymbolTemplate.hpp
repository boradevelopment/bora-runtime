#pragma once
#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include "wasm_export.h" // For NativeSymbol, etc.

class HostSymbolTemplate {
public:
    virtual ~HostSymbolTemplate() = default;

    // Returns the module/namespace name
    virtual const char* get_namespace() const = 0;

    // Returns the list of native symbols (to register)
    virtual const std::vector<NativeSymbol>& get_symbols() const = 0;

    // Registers the symbols with the WASM runtime
    void register_symbols() const {
        const char *module = get_namespace();
        const std::vector<NativeSymbol> &symbols = get_symbols();  // get by const reference

        // wasm_runtime_register_natives expects pointer to NativeSymbol array and size
        wasm_runtime_register_natives(module, const_cast<NativeSymbol *>(symbols.data()), symbols.size());
    }
    };
