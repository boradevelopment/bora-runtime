#include "guestImports.h"

#include "bora/Commands.h"
#include "bora/boraWindowSymbols.h"
//#include "stl/atomic.h"
#include "stl/faults.h"
#include "stl/file.h"
#include "stl/wasi.h"
#include "stl/thread.h"

// Its hacky but it should work for the demo
void guestImports::initalizeSymbols() {
    registers->add(std::make_unique<BoraThreadSymbols>());
   // registers->add(std::make_unique<BoraStringSymbols>()); - DEPRECATED - 6/13/2025
    //registers->add(std::make_unique<boraAtomicSymbols>());
    registers->add(std::make_unique<HostCommandSymbols>());
    registers->add(std::make_unique<WasiHostSymbols>());
    registers->add(std::make_unique<EnvFaultHostSymbols>());
    registers->add(std::make_unique<BoraWindowSymbols>());
    registers->add(std::make_unique<FileHostSymbols>());
}

guestImports::guestImports() {
    registers = std::make_unique<guestImportsRegister>();
}

void guestImports::registerSymbol(wasmtime_linker_t* linker) {
    registers->register_all(linker);
}

