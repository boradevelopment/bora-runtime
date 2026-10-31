#pragma once
// #include "stl/thread.h"
#include "templates/guestImportsRegister.h"

class guestImports {
public:
    guestImports();
public:
    void initalizeSymbols();
    void registerSymbol(wasmtime_linker_t* linker);
private:
    std::unique_ptr<guestImportsRegister> registers;
};


