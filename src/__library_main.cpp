#include <cstring>
#include "runtime/__runtime_main.h"
#include "supervisor/__supervisor_main.h"

int
main(int argc, char *argv[])
{
auto rootLogger = LogManager::instance().getLogger("root");
#ifndef NDEBUG // debug builds have a verbosity of 3 by default
    rootLogger->setVerbosityLimit(3);
#endif

#ifdef BORA_ENABLE_SUPERVISOR
    if (argc >= 2 && strcmp(argv[1], "as-supervisor") == 0)
    {
        return supervisor_main(argc, argv);
    }
#endif
    return runtime_main(argc, argv);
}