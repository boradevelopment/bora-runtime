#pragma once
#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include "wasmtime.hh" // For NativeSymbol, etc.

class HostSymbolTemplate {
public:
    virtual ~HostSymbolTemplate() = default;

    // Returns the module/namespace name
    virtual const char* get_namespace() const = 0;

    virtual void bind_symbols(wasmtime_linker_t* linker) const = 0;

protected:
    void handle_error(wasmtime_error_t* error) const {
        wasm_name_t msg;
        wasmtime_error_message(error, &msg);
        std::cerr << "Symbol Binding Error: " << std::string(msg.data, msg.size) << "\n";
        wasm_name_delete(&msg);
        wasmtime_error_delete(error);
    }
    };
