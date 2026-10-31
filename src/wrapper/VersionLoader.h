#include <iostream>
#ifdef WIN32
#include "Windows.h"
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#include <string>

class VersionLoader {
public:
    // Static function to load and execute a dylib
    static int executeLatestVersion(int argc, char** argv);
    static int loadVersion(const wchar_t* path, int argc, char** argv);
};