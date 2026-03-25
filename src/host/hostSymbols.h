#include "stl/thread.h"
#include "hostSymbolRegister.h"

class hostSymbols {
public:
    hostSymbols();
public:
    void initalizeSymbols();
    void registerSymbol();
private:
    hostSymbolRegister* registers;
};


