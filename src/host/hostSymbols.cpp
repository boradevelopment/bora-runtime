#include "hostSymbols.h"

#include "Commands.h"
#include "bora/boraLoggingSymbols.h"
#include "stl/string.h"
#include "bora/boraMemorySymbols.h"
#include "stl/atomic.h"

// Its hacky but it should work for the demo
void hostSymbols::initalizeSymbols() {
    registers->add(std::make_unique<BoraThreadSymbols>());
    registers->add(std::make_unique<BoraInputSymbols>());
    registers->add(std::make_unique<BoraLoggingSymbols>());
    registers->add(std::make_unique<BoraMemorySymbols>());
   // registers->add(std::make_unique<BoraStringSymbols>()); - DEPRECATED - 6/13/2025
    registers->add(std::make_unique<boraAtomicSymbols>());
    registers->add(std::make_unique<HostCommandSymbols>());
}

hostSymbols::hostSymbols() {
    registers = new hostSymbolRegister();
}

void hostSymbols::registerSymbol() {
    registers->register_all();
}

