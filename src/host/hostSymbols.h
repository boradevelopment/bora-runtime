#pragma once
#include "stl/thread.h"
#include "hostSymbolRegister.h"

class hostSymbols {
public:
    hostSymbols();
public:
    void initalizeSymbols();
    void registerSymbol(wasmtime_linker_t* linker);
private:
    hostSymbolRegister* registers;
};


