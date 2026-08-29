#include "VersionLoader.h"

int VersionLoader:: executeLatestVersion(int argc, char **argv) {
    #ifdef WASM_ENABLE_DEBUG_INTERP
    #define LIB_SUFFIX "D"
#else
    #define LIB_SUFFIX ""
#endif

#ifdef __APPLE__
    const char* libPath = "compatibility/latest" LIB_SUFFIX ".dylib";
#elif defined(WIN32)
    const char* libPath = "compatibility/LATEST" LIB_SUFFIX ".dll";
#endif

    return loadVersion(libPath, argc, argv);
}

int VersionLoader::loadVersion(const char *path, int argc, char **argv) {
#ifdef __APPLE__
    void* handle = dlopen(path, RTLD_LAZY);

    if (!handle) {
        std::cerr << "Failed to load library: " << dlerror() << std::endl;
        return 1;
    }
    using MainFn = int(*)(int, char**);
    MainFn dllMain = reinterpret_cast<MainFn>(dlsym(handle, "main"));

    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "`main` function not found: " << dlsym_error << std::endl;
        dlclose(handle);
        return 1;
    }
    int result = dllMain(argc, argv);
    dlclose(handle);
    return result;
#elif defined(WIN32)
    HMODULE handle = LoadLibraryA(path);

    if (!handle) {
        std::cerr << "Failed to load library: " << GetLastError() << std::endl;
        return 1;
    }

    using MainFn = int(*)(int, char**);

    MainFn dllMain = reinterpret_cast<MainFn>(
        GetProcAddress(handle, "main")
    );

    if (!dllMain) {
        std::cerr << "`main` function not found: " << GetLastError() << std::endl;
        FreeLibrary(handle);
        return 1;
    }

    int result = dllMain(argc, argv);

    FreeLibrary(handle);
    return result;
#endif
}
