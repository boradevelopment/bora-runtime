#include <iostream>
#ifdef WIN32
#include "Windows.h"
#endif
#ifdef __APPLE__
#include <dlfcn.h>
#endif
#include <string>

class VersionLoader {
public:
    // Static function to load and execute a dylib
    static int executeLatestVersion(int argc, char** argv);
    static int loadVersion(const char* path, int argc, char** argv);
};