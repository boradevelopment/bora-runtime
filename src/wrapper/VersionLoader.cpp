#include "VersionLoader.h"
#include "TypeDefinitions.h"
#include "tools/DynamicLibrary.h"

int VersionLoader:: executeLatestVersion(int argc, char **argv) {
    sWstring libPath = L"compatibility/bora.latest";
#ifdef __APPLE__
     libPath.append("L.dylib");
#elif defined(WIN32)
    libPath.append(L".dll");
#elif defined(__linux__)
    libPath.append(L".so");
#endif

    return loadVersion(libPath.c_str(), argc, argv);
}

int VersionLoader::loadVersion(const wchar_t *path, int argc, char **argv) {
    DynamicLibrary library(path);

    if (library.hasOpened())
    {
      using MainFn = int(*)(int, char**);
      auto libMain = reinterpret_cast<MainFn>(library.getProcAddress("main"));
      if (libMain != nullptr)
      {
          return libMain(argc, argv);
      }
    }

    return -1;
//
// #if defined(__linux__) || defined(__APPLE__)
//     void* handle = dlopen(path, RTLD_LAZY);
//
//     if (!handle) {
//         std::cerr << "Failed to load library: " << dlerror() << std::endl;
//         return 1;
//     }
//     using MainFn = int(*)(int, char**);
//     auto dllMain = reinterpret_cast<MainFn>(dlsym(handle, "main"));
//     if (const char* dlsym_error = dlerror()) {
//         std::cerr << "`main` function not found: " << dlsym_error << std::endl;
//         dlclose(handle);
//         return 1;
//     }
//
//     int result = dllMain(argc, argv);
//     dlclose(handle);
//     return result;
// #elif defined(WIN32)
//     HMODULE handle = LoadLibraryA(path);
//
//     if (!handle) {
//         std::cerr << "Failed to load library: " << GetLastError() << std::endl;
//         return 1;
//     }
//
//     using MainFn = int(*)(int, char**);
//
//     MainFn dllMain = reinterpret_cast<MainFn>(
//         GetProcAddress(handle, "main")
//     );
//
//     if (!dllMain) {
//         std::cerr << "`main` function not found: " << GetLastError() << std::endl;
//         FreeLibrary(handle);
//         return 1;
//     }
//
//     int result = dllMain(argc, argv);
//
//     FreeLibrary(handle);
//     return result;
// #endif

}
