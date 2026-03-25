#include "hostSymbols.h"

#include "Commands.h"
#include "stl/string.h"
#include "bora/boraWindowSymbols.h"
#include "stl/atomic.h"
#include "stl/faults.h"
#include "stl/wasi.h"

// Its hacky but it should work for the demo
void hostSymbols::initalizeSymbols() {
    registers->add(std::make_unique<BoraThreadSymbols>());
   // registers->add(std::make_unique<BoraStringSymbols>()); - DEPRECATED - 6/13/2025
    registers->add(std::make_unique<boraAtomicSymbols>());
    registers->add(std::make_unique<HostCommandSymbols>());
    registers->add(std::make_unique<WasiHostSymbols>());
    registers->add(std::make_unique<EnvFaultHostSymbols>());
    registers->add(std::make_unique<BoraWindowSymbols>());
}

hostSymbols::hostSymbols() {
    registers = new hostSymbolRegister();
}

void hostSymbols::registerSymbol() {
    registers->register_all();
}

