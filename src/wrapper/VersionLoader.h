#include <iostream>
#include <dlfcn.h>
#include <string>

class VersionLoader {
public:
    // Static function to load and execute a dylib
    static int executeLatestVersion(int argc, char** argv) {
#ifdef __APPLE__
        const char* libPath = "compatibility/latest.dylib";
#endif

        loadVersion(libPath, argc, argv);
    }

    static int loadVersion(const char* path, int argc, char** argv) {
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
#endif
    }
};