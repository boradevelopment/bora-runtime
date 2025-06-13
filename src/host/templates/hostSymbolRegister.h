#pragma once
#include <vector>
#include <memory>
#include "hostSymbolTemplate.hpp" // base class

class hostSymbolRegister {
public:
    hostSymbolRegister();
public:
    // Adds a module (symbol registrar) to the list
    void add(std::unique_ptr<HostSymbolTemplate> module);

    // Calls register_symbols on all modules
    void register_all();

private:
    // Store modules in a static vector
    std::vector<std::unique_ptr<HostSymbolTemplate>>& get_modules();
};
