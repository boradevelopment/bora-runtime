//
// Created by isloe on 6/8/2025.
//

#include "guestImportsRegister.h"

void guestImportsRegister::add(std::unique_ptr<HostSymbolTemplate> module) {
    get_modules().emplace_back(std::move(module));
}

void guestImportsRegister::register_all(wasmtime_linker_t* linker) {
    for (const auto& module : get_modules()) {
        module->bind_symbols(linker);
    }
}

std::vector<std::unique_ptr<HostSymbolTemplate>> &guestImportsRegister::get_modules(){
    static std::vector<std::unique_ptr<HostSymbolTemplate>> modules;
    return modules;
}

guestImportsRegister::guestImportsRegister() {

}
