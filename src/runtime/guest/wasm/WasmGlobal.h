// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
/* 
 * FileName: WasmLibraryLinker.h
 * Purpose: Manages runtime libraries instances
 */

#pragma once
#include <wasmtime.hh>

#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <stdexcept>

#include "WasmRunner.h"
#include "WasmSupervisorManager.h"
#include "logging/Logger.h"
#include "logging/LogManager.h"
#include "logging/LogStream.h"
#include "nNetworking/DomainSocketFactory.h"
#include "nNetworking/IDomainSocket.h"

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#else
    #include <unistd.h>
#endif

namespace fs = std::filesystem;

class WasmGlobal
{
bool isDebugging = false;
bwasmtime_gdb_server_t* debugServer_ = nullptr;
wasmtime_context_t* context_ = nullptr;
wasm_engine_t* engine_ = nullptr;
wasmtime_store_t* store_ = nullptr;
wasmtime_linker_t* linker_ = nullptr;
wasmtime_sharedmemory_t* sharedMemory_ = nullptr;
const wasmtime_table_t* sharedTable_ = nullptr;
const wasmtime_global_t* globalStack_ = nullptr;
std::unordered_set<std::string> loaded_libraries_;
std::unordered_map<std::string, const WasmRuntimeModule*> library_instances_;
const WasmRuntimeModule* mainModule = nullptr;
std::shared_ptr<Logger> logger_;
guestImports* globalSymbols = nullptr;
fs::path application_dir_;
std::atomic<i64> m_currentMemoryTail = (1 * 1024 * 1024);
sUniquePointer<IMappedMemory> mapped_memory_;

// todo: move this when we need it somewhere else?
static fs::path GetUserLibraryDirectory() {
    fs::path libDir;

#if defined(_WIN32)
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData) {
        libDir = fs::path(localAppData) / "bora" / "libraries";
    } else {
        libDir = fs::temp_directory_path() / "bora" / "libraries";
    }
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        libDir = fs::path(home) / "Library" / "Caches" / "bora" / "libraries";
    } else {
        libDir = fs::temp_directory_path() / "bora" / "libraries";
    }
#else // Linux / POSIX
    const char* xdgCache = std::getenv("XDG_CACHE_HOME");
    if (xdgCache && xdgCache[0] != '\0') {
        libDir = fs::path(xdgCache) / "bora" / "libraries";
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            libDir = fs::path(home) / ".cache" / "bora" / "libraries";
        } else {
            libDir = fs::temp_directory_path() / "bora" / "libraries";
        }
    }
#endif
    return libDir;
}

// 2. System-wide native runtime directory (where the BORA executable/runtime binary lives)
static fs::path GetSystemRuntimeDirectory() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (len > 0) {
        return fs::path(buf).parent_path();
    }
#elif defined(__APPLE__)
    char buf[1024];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        return fs::canonical(buf).parent_path();
    }
#else // Linux / POSIX
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        return fs::path(buf).parent_path();
    }
#endif
    return fs::current_path(); // Fallback
}

sUnorderedMap<std::string, int64_t> module_memory_bases_;
sUnorderedMap<std::string, int64_t> module_table_bases_;
// Memory offset starts above the main app's stack area (e.g. 2 MB offset: 2097152)
u64 current_memory_offset_ = m_currentMemoryTail;
u64 current_table_offset_ = 1;
std::mutex linker_mutex_;
sUniquePointer<WasmSupervisorManager> supervisor_ = nullptr;

static WasmGlobal* mgr;

static constexpr int64_t align_to(int64_t offset, int64_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

 uint64_t AlignUp(uint64_t value, uint32_t alignmentExponent) const {
    uint64_t alignment = 1ULL << alignmentExponent; // 2^alignment
    uint64_t mask = alignment - 1;
    return (value + mask) & ~mask;
}


public:
    std::mutex mtx;
    std::condition_variable was_filedescriptor_received;
    WasmGlobal()
    = default;

    WasmGlobal(wasmtime_context_t* context, wasm_engine_t* engine, wasmtime_store_t* store, fs::path app_dir = fs::current_path());

    static WasmGlobal* getInstance() {
        return mgr;
    }

    ~WasmGlobal()
    = default;

    wasmtime_context_t* getContext() const {return context_; }
    wasm_engine_t* getEngine() const {return engine_; }
    wasmtime_store_t* getStore() const {return store_;}
    bwasmtime_gdb_server_t* getDebugServer() const {return debugServer_;}
    bool getDebugging() const {return isDebugging;}

    WasmSupervisorManager* getSupervisor() const {return supervisor_.get(); }
    IMappedMemory* getMainMemoryMapping() const {return mapped_memory_.get(); }

    wasmtime_linker_t* getLinker() const { return linker_; }
    std::shared_ptr<Logger> getLogger() { return logger_; }
    const wasmtime_table_t* getGlobalTable() const { return sharedTable_; }
    wasmtime_sharedmemory_t* getGlobalSharedMemory() const { return sharedMemory_; }
    const wasmtime_global_t* getGlobalStack() const { return globalStack_; }
    const WasmRuntimeModule* getMainModule() const { return mainModule; }

    void enableDebugging(const wasm_config_t* config, const char* debugAddress);

    void setMappedMemory(sUniquePointer<IMappedMemory> memory)
    {
        mapped_memory_ = std::move(memory);
    }

    void setGlobalSharedMemory(wasmtime_sharedmemory_t* memory)
    {
        if (sharedMemory_ == nullptr) sharedMemory_ = memory;
    }
    void setGlobalTable(const wasmtime_table_t* table)
    {
        if (sharedTable_ == nullptr) sharedTable_ = table;
    }
    void setGlobalStackPointer(const wasmtime_global_t* stackPointer)
    {
        if (globalStack_ == nullptr) globalStack_ = stackPointer;
    }

    void setMainModule(const WasmRuntimeModule* module)
    {
        mainModule = module;
    }

    i64 getTail() const { return AlignUp(m_currentMemoryTail, 16); }

    /// @brief Safety Check: Ensures all imports requested by a module exist in the Linker
    /// @param module The compiled WASM dependency module to inspect
    bool validateImports(const wasmtime_module_t* module, const std::string& moduleName) const;

    /// @brief Gets missing imports that havent been satisfied
    /// @param module The compiled WASM dependency module to inspect
    sVec<sString> getMissingImports(const wasmtime_module_t* module) const;
    const WasmThreadInstance*  registerRuntimeLibraryForThread(wasmtime_store_t* store, wasmtime_context_t* context, const WasmRuntimeModule* dependencyModule,
                                         u64 stackOffset);

/// @brief Registers a dynamic library module into the runtime linker
    /// @param lib_name The namespace this library exports (e.g., "libbora_graphics")
    /// @param wasm_bytes The raw WASM byte buffer of the dynamic library
    bool registerRuntimeLibrary(const std::string& lib_name, const std::string& path, bool skipReqDeps = false);
    fs::path findLibrary(const std::string& libraryName) const;

    int64_t get_module_memory_base(const std::string& appId, uint32_t required_alignment, int64_t estimated_data_size = 65536);
    int64_t get_module_table_base(const std::string& appId, uint32_t table_alignment, int64_t estimated_table_slots = 64);
    void addOnTail(u64 memorySize);
    void removeFromTail(u64 memorySize);
    void destroy();

    WasmGlobal& operator=(WasmGlobal&& other) noexcept {
        if (this != &other) {
            destroy();

            context_ = other.context_;
            engine_ = other.engine_;
            store_ = other.store_;
            application_dir_ = std::move(other.application_dir_);
            linker_ = other.linker_;
            globalSymbols = other.globalSymbols;
            logger_ = std::move(other.logger_);
            supervisor_ = std::move(other.supervisor_);
            other.context_ = nullptr;
            other.engine_ = nullptr;
            other.store_ = nullptr;
            other.linker_ = nullptr;
            other.globalSymbols = nullptr;
            mgr = this;
        }
        return *this;
    }

std::list<const WasmRuntimeModule*> getLibraries();
void setTail(i64 tail);

static i64 getCommonStackSize()
{
    return 1 * 1024 * 1024;
}

WasmGlobal(WasmGlobal&& other) noexcept {
        *this = std::move(other);
    }
};

