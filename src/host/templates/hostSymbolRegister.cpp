//
// Created by isloe on 6/8/2025.
//

#include "hostSymbolRegister.h"

void hostSymbolRegister::add(std::unique_ptr<HostSymbolTemplate> module) {
    get_modules().emplace_back(std::move(module));
}

void hostSymbolRegister::register_all(wasmtime_linker_t* linker) {
    for (const auto& module : get_modules()) {
        module->bind_symbols(linker);
    }
}

std::vector<std::unique_ptr<HostSymbolTemplate>> &hostSymbolRegister::get_modules(){
    static std::vector<std::unique_ptr<HostSymbolTemplate>> modules;
    return modules;
}

hostSymbolRegister::hostSymbolRegister() {

}
