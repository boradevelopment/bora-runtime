//
// Created by isloe on 6/8/2025.
//

#include "hostSymbolRegister.h"

void hostSymbolRegister::add(std::unique_ptr<HostSymbolTemplate> module) {
    get_modules().emplace_back(std::move(module));
}

void hostSymbolRegister::register_all() {
    for (const auto& module : get_modules()) {
        module->register_symbols();
    }
}

std::vector<std::unique_ptr<HostSymbolTemplate>> &hostSymbolRegister::get_modules(){
    static std::vector<std::unique_ptr<HostSymbolTemplate>> modules;
    return modules;
}

hostSymbolRegister::hostSymbolRegister() {

}
