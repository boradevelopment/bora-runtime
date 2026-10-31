// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

#include "WasmRunner.h"

#include <cstring>
#include <sys/mman.h> // todo: abstract!

#include "WasmCacheManager.h"
#include "guest/templates/LinkerTemplate.h"
#include "guest/wasm/WasmTools.h"
#include "guest/stl/thread.h"
#include "logging/Logger.h"
#include "logging/LogManager.h"
#include "logging/LogStream.h"
#include "tools/CPUInfo.h"
#include "WasmGlobal.h"
#include "WasmRegistry.h"
#include "memory/MappedMemoryFactory.h"
#include "networking/supervisor/codecs/FileDescriptor.h"

namespace fs = std::filesystem;

// Get platform-appropriate cache directory
static fs::path GetBoraCacheDirectory() {
    fs::path cacheDir;

#if defined(_WIN32)
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData) {
        cacheDir = fs::path(localAppData) / "bora" / "cache";
    } else {
        cacheDir = fs::temp_directory_path() / "bora" / "cache";
    }
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        cacheDir = fs::path(home) / "Library" / "Caches" / "bora";
    } else {
        cacheDir = fs::temp_directory_path() / "bora" / "cache";
    }
#else // Linux / POSIX
    const char* xdgCache = std::getenv("XDG_CACHE_HOME");
    if (xdgCache && xdgCache[0] != '\0') {
        cacheDir = fs::path(xdgCache) / "bora";
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            cacheDir = fs::path(home) / ".cache" / "bora";
        } else {
            cacheDir = fs::temp_directory_path() / "bora" / "cache";
        }
    }
#endif

    // Ensure directory exists
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    return cacheDir;
}

// todo: move somewhere?
static std::string ComputeWasmChecksum(const std::vector<uint8_t>& wasmBytes) {
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    for (uint8_t byte : wasmBytes) {
        hash ^= byte;
        hash *= 1099511628211ULL; // FNV prime
    }

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

// Helper to sanitize filename fallback if ID variable isn't present
static std::string ExtractAppIDOrFallback(const V2Archive& boraApp, const char* tazaFileName) {
    // 1. Try finding 'ID' or 'id' in customVariables
    auto idIt = boraApp.header.customVariables.find(L"ID");
    if (idIt == boraApp.header.customVariables.end()) {
        idIt = boraApp.header.customVariables.find(L"id");
    }

    if (idIt != boraApp.header.customVariables.end()) {
        try {
            std::string idStr = std::get<std::string>(idIt->second);
            if (!idStr.empty()) return idStr;
        } catch (...) {
            // Variant type mismatch fallback
        }
    }

    // 2. Fallback: Use file stem from tazaFileName
    fs::path p(tazaFileName);
    return p.stem().string();
}

// Creates a WASMTIME Module from WASM Binary Data (Not AOT/CWASM)
static wasmtime_module_t* CreateModuleFromWASMMemory(wasm_engine_t *engine, u8* data, u64 size)
{
    wasm_byte_vec_t wasm_bytes;
    wasm_byte_vec_new(
        &wasm_bytes,
        size,
        reinterpret_cast<const wasm_byte_t*>(data)
    );

    wasmtime_module_t *module = nullptr;
    wasmtime_error_t *error = nullptr;

    error = wasmtime_module_new(
        engine,
        reinterpret_cast<const uint8_t*>(wasm_bytes.data),
        wasm_bytes.size,
        &module
    );

    wasm_byte_vec_delete(&wasm_bytes);

    if (error != nullptr) {
        wasmtime_error_delete(error);
        return nullptr;
    }

    return module;
}

static wasmtime_module_t* CreateModuleFromAOTMemory(wasm_engine_t *engine, u8* data, u64 size)
{
    wasm_byte_vec_t aot_bytes;
    wasm_byte_vec_new(
        &aot_bytes,
        size,
        reinterpret_cast<const wasm_byte_t*>(data)
    );

    wasmtime_module_t *module = nullptr;
    wasmtime_error_t *error = nullptr;

    error = wasmtime_module_deserialize(engine, (uint8_t *)aot_bytes.data, aot_bytes.size, &module);
    wasm_byte_vec_delete(&aot_bytes);

    WasmTools::logandDeleteWASMFail(error, "AOT Load");

    return module;
}

static bool read_file(const char *path, wasm_byte_vec_t *out_vector) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    wasm_byte_vec_new_uninitialized(out_vector, size);
    if (fread(out_vector->data, 1, size, f) != size) {
        fclose(f);
        wasm_byte_vec_delete(out_vector);
        return false;
    }

    fclose(f);
    return true;
}

void WasmRunner::clearCache()
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(GetBoraCacheDirectory(), ec)) {
        fs::remove_all(entry.path(), ec);
    }
}

bool get_module_memory_limits(const wasmtime_module_t* module, wasm_limits_t* out_limits) {
    if (!module || !out_limits) return false;

    // 1. Check if the module IMPORTS memory (e.g. from "env::memory")
    wasm_importtype_vec_t imports;
    wasmtime_module_imports(module, &imports);

    for (size_t i = 0; i < imports.size; ++i) {
        const wasm_externtype_t* extern_type = wasm_importtype_type(imports.data[i]);
        if (wasm_externtype_kind(extern_type) == WASM_EXTERN_MEMORY) {
            const wasm_memorytype_t* mem_type = wasm_externtype_as_memorytype_const(extern_type);
            const wasm_limits_t* limits = wasm_memorytype_limits(mem_type);
            *out_limits = *limits;
            wasm_importtype_vec_delete(&imports);
            return true;
        }
    }
    wasm_importtype_vec_delete(&imports);

    // 2. Check if the module EXPORTS memory (e.g. "memory")
    wasm_exporttype_vec_t exports;
    wasmtime_module_exports(module, &exports);

    for (size_t i = 0; i < exports.size; ++i) {
        const wasm_externtype_t* extern_type = wasm_exporttype_type(exports.data[i]);
        if (wasm_externtype_kind(extern_type) == WASM_EXTERN_MEMORY) {
            const wasm_memorytype_t* mem_type = wasm_externtype_as_memorytype_const(extern_type);
            const wasm_limits_t* limits = wasm_memorytype_limits(mem_type);
            *out_limits = *limits;
            wasm_exporttype_vec_delete(&exports);
            return true;
        }
    }
    wasm_exporttype_vec_delete(&exports);

    return false; // No memory declared in imports or exports
}

uint32_t ReadULEB128(const uint8_t*& ptr, const uint8_t* end) {
    uint32_t result = 0;
    uint32_t shift = 0;
    while (ptr < end) {
        uint8_t byte = *ptr++;
        result |= (byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return result;
}

uint32_t EstimateTableSlotsFromElements(const std::vector<uint8_t>& wasmBytes) {
    if (wasmBytes.size() < 8) return 0;
    const uint8_t* ptr = wasmBytes.data() + 8;
    const uint8_t* end = wasmBytes.data() + wasmBytes.size();

    uint32_t totalElementSlots = 0;

    while (ptr < end) {
        uint8_t sectionId = *ptr++;
        uint32_t sectionLen = ReadULEB128(ptr, end);
        const uint8_t* sectionEnd = ptr + sectionLen;

        if (sectionId == 9) { // Element Section
            uint32_t numSegments = ReadULEB128(ptr, sectionEnd);
            for (uint32_t i = 0; i < numSegments; ++i) {
                uint32_t flags = ReadULEB128(ptr, sectionEnd);

                // Active segments write functions into table at __table_base
                // Read vector length of elements in segment
                if ((flags & 1) == 0) { // Active segment offset expression
                    // Skip core expression (usually i32.const / global.get + end)
                    while (ptr < sectionEnd && *ptr != 0x0B) ptr++;
                    if (ptr < sectionEnd) ptr++; // Skip 0x0B (end)
                }

                uint32_t numElements = ReadULEB128(ptr, sectionEnd);
                totalElementSlots += numElements;
                break; // Sum total element counts across segments
            }
            return totalElementSlots;
        }
        ptr = sectionEnd;
    }
    return 0;
}

uint32_t EstimateMemorySizeFromDataSection(const std::vector<uint8_t>& wasmBytes) {
    if (wasmBytes.size() < 8) return 0;

    const uint8_t* ptr = wasmBytes.data() + 8;
    const uint8_t* end = wasmBytes.data() + wasmBytes.size();

    uint32_t totalDataBytes = 0;

    while (ptr < end) {
        uint8_t sectionId = *ptr++;
        uint32_t sectionLen = ReadULEB128(ptr, end);
        const uint8_t* sectionEnd = ptr + sectionLen;

        if (sectionId == 11) { // Section 11: Data Section
            uint32_t numSegments = ReadULEB128(ptr, sectionEnd);

            for (uint32_t i = 0; i < numSegments; ++i) {
                uint32_t flags = ReadULEB128(ptr, sectionEnd);

                // Active segments specify a memory index / core expression
                if ((flags & 1) == 0) {
                    // Active segment: Skip core expression (e.g., i32.const / global.get + 0x0B)
                    while (ptr < sectionEnd && *ptr != 0x0B) ptr++;
                    if (ptr < sectionEnd) ptr++; // Skip 0x0B (end opcode)
                } else if (flags == 1) {
                    // Passive segment (no offset expression, used by memory.core)
                } else if (flags == 2) {
                    // Active segment with explicit memory index
                    ReadULEB128(ptr, sectionEnd); // Skip memory index
                    while (ptr < sectionEnd && *ptr != 0x0B) ptr++;
                    if (ptr < sectionEnd) ptr++; // Skip 0x0B
                }

                // Read data payload byte count
                uint32_t dataByteLen = ReadULEB128(ptr, sectionEnd);
                totalDataBytes += dataByteLen;

                // Skip raw payload bytes
                ptr += dataByteLen;
            }
            return totalDataBytes;
        }
        ptr = sectionEnd;
    }
    return 0;
}

struct DylinkInfo {
    uint32_t mem_size = 0;
    uint32_t mem_align = 0;
    uint32_t table_size = 0;
    uint32_t table_align = 0;
    bool found = false;
};

DylinkInfo ParseDylinkSection(const std::vector<uint8_t>& wasmBytes) {
    DylinkInfo info;
    if (wasmBytes.size() < 8) return info;

    const uint8_t* ptr = wasmBytes.data() + 8; // Skip 4-byte magic + 4-byte version
    const uint8_t* end = wasmBytes.data() + wasmBytes.size();

    while (ptr < end) {
        uint8_t sectionId = *ptr++;
        uint32_t sectionLen = ReadULEB128(ptr, end);
        const uint8_t* sectionEnd = ptr + sectionLen;

        // Section 0 is Custom Section
        if (sectionId == 0) {
            uint32_t nameLen = ReadULEB128(ptr, sectionEnd);
            std::string sectionName(reinterpret_cast<const char*>(ptr), nameLen);
            ptr += nameLen;

            // Check for WASM32 "dylink" or WASM64 "dylink.0"
            if (sectionName == "dylink" || sectionName == "dylink.0") {
                info.found = true;

                // Parse sub-sections inside dylink
                while (ptr < sectionEnd) {
                    uint8_t subType = *ptr++;
                    uint32_t subLen = ReadULEB128(ptr, sectionEnd);
                    const uint8_t* subEnd = ptr + subLen;

                    // WASM_DYLINK_MEM_INFO (subType 1) contains mem_size & mem_align
                    if (subType == 1) {
                        info.mem_size = ReadULEB128(ptr, subEnd);
                        info.mem_align = ReadULEB128(ptr, subEnd);
                        info.table_size = ReadULEB128(ptr, subEnd);
                        info.table_align = ReadULEB128(ptr, subEnd);
                        break;
                    }
                    ptr = subEnd;
                }
                return info;
            }
        }
        ptr = sectionEnd;
    }
    return info;
}

const WasmRuntimeModule* WasmRunner::loadModuleByTAZA(WasmGlobal* linker, wasmtime_store_t* store, wasmtime_context_t* context, wasm_engine_t* engine,
                                                      const char* tazaFileName, wasmtime_sharedmemory_t* shared_memory, const wasmtime_global_t* globalStack, bool isRecursed)
{
    auto logInstance = LogManager::instance().getLogger("bora.module");
    V2Archive boraApp;
    boraApp.output = tazaFileName;
    std::vector<uint8_t> vData;

    int resArchive = boraApp.getArchive();
    if(resArchive != 0){ // verbose logging todo
        LOG_ERROR_VERBOSE(logInstance, 1) << "TAZA archive could not be loaded at all.";
        return nullptr;
    } else {
        // todo: unsafe?
        if(std::get<std::string>(boraApp.header.customVariables[L"magic"]) != "BORA"){
            LOG_ERROR_VERBOSE(logInstance, 1) << "Magic is not for an BORA Application nor an ";
            return nullptr;
        }
        auto entryFile = std::get<std::wstring>(boraApp.header.customVariables[L"entry"]);
        if(entryFile.empty()){
            LOG_ERROR_VERBOSE(logInstance, 1) << "No entry point in header variables!";
            return nullptr;
        }

        auto entryV2File = boraApp.header.files.find(entryFile);
        if(entryV2File == boraApp.header.files.end()){
            LOG_ERROR_VERBOSE(logInstance, 1) << "Entry point is not a valid file in TAZA.";
            return nullptr;
        }
        std::ifstream boraAppInputStream(tazaFileName, std::ios::in | std::ios::binary);
        size_t fileSize = entryV2File->second.size;
        auto mappedMemory = MappedMemoryFactory::allocate(fileSize);
        bool result = boraApp.header.getV2FileDirect(
            mappedMemory->getData(),
            boraAppInputStream,
            entryV2File->second,
            boraApp.iv,
            boraApp.key
        );
        if (!result) return nullptr;
        vData = sVec<u8>(mappedMemory->getData(), mappedMemory->getData() + entryV2File->second.size);
        if (vData.empty())
        {
            LOG_ERROR_VERBOSE(logInstance, 1) << "Entry point data has nothing.";
            return nullptr; // what are we going to do nothing?
        }
        std::string appId = ExtractAppIDOrFallback(boraApp, tazaFileName);
        std::string checksum = WasmCacheManager::ComputeWasmChecksum(vData);
        fs::path cacheDir = GetBoraCacheDirectory();

        auto* module = new WasmRuntimeModule(appId, context, store); // register as soon as its created to prevent issues.
        module->mainThread.store = store;
        module->mainThread.context = context;
        module->module = nullptr;
        module->archiveMap = std::move(mappedMemory);

        if (WasmCacheManager::doesCacheExist(appId.c_str(), checksum.c_str()) && !linker->getDebugging())
        {
            LOG_INFO_VERBOSE(logInstance, 2) << "Cache file was found! App ID: " << appId << " Checksum: " << checksum;
            auto cacheBytes = WasmCacheManager::loadAppFromCache(appId.c_str(), checksum.c_str());
            if (!cacheBytes.empty()) {
                module->module = CreateModuleFromAOTMemory(
                    engine,
                    cacheBytes.data(),
                    cacheBytes.size()
                );
            }

            // If deserialization failed (e.g. corrupted cache file), delete it so we recompile
            if (module->module == nullptr) {
                LOG_WARN_VERBOSE(logInstance, 1) << "Cache file is invalid, WASM will be reserialized.";
                WasmCacheManager::invalidateAppHash(appId.c_str(), checksum.c_str());
            }
        }

        if (module->module == nullptr) {
            if (vData.empty()) {
                delete module;
                return nullptr;
            }

            // Compile raw WASM
            module->module = CreateModuleFromWASMMemory(engine, vData.data(), vData.size());
            if (module->module == nullptr) {
                LOG_ERROR_VERBOSE(logInstance, 1) << "WASM file is invalid as module could not be created.";
                delete module;
                return nullptr;
            }

            if (!linker->getDebugging()) WasmCacheManager::saveBytesToCache(appId.c_str(), &vData, module->module);
        }
        if (!linker->getMainModule())
        {
            linker->setMainModule(module);
        }
        vec8 buffer;
        SupervisorCodecs::GeneralFileDescriptor(std::make_unique<SupervisorCodecs::TAZAArchiveMappingFileDescriptor>(appId)).serialize(buffer);
        linker->getSupervisor()->communicate(SendFileDescriptor, buffer.data(), buffer.size());
        {
            std::unique_lock<std::mutex> lock(linker->mtx);
            linker->was_filedescriptor_received.wait(lock);
        }

        linker->getSupervisor()->getSocket()->send_fd(module->archiveMap->getFileDescriptor());
        {
            std::unique_lock<std::mutex> lock(linker->mtx);
            linker->was_filedescriptor_received.wait(lock);
        }

        if (linker->getDebugging()) bwasmtime_store_debug_register_module(linker->getStore(), module->module);
        if (appMemoryLimits.min == 0)
        {
            if (get_module_memory_limits(module->module, &appMemoryLimits)) {
                if (appMemoryLimits.max == 0 || appMemoryLimits.max == UINT32_MAX) {
                    appMemoryLimits.max = 65536;
                }
            }
        }

        auto logoV2File = boraApp.header.files.find(L"logo");
        if (logoV2File != boraApp.header.files.end()) module->logoData = boraApp.header.getV2File(boraAppInputStream,logoV2File->second, boraApp.iv, boraApp.key);

        DylinkInfo dylink = ParseDylinkSection(vData);
        int32_t slots = dylink.found ? dylink.table_size : EstimateTableSlotsFromElements(vData);
        uint32_t memoryDataSize = dylink.found ? dylink.mem_size
                                              : (EstimateMemorySizeFromDataSection(vData) + 4096);
        linker->addOnTail(memoryDataSize);
        wasmtime_error_t *error = NULL;
        for (auto libraryName : linker->getMissingImports(module->module))
        {
           fs::path libraryPath = linker->findLibrary(libraryName);
            if (fs::exists(libraryPath))
            {
             if (!linker->registerRuntimeLibrary(libraryName, libraryPath.generic_string()))
             {
                 LOG_ERROR(logInstance) << "Runtime library " << libraryName << " could not be registered! Contact the developers of the library!!!";
                 return nullptr;
             }
            } else
            {
                LOG_ERROR(logInstance) << "Missing required runtime library " << libraryName << " not found!";
                return nullptr;
            }
        }
        if (!linker->validateImports(module->module, appId))
        {
            return nullptr;
        }


        if (shared_memory == nullptr) shared_memory = linker->getGlobalSharedMemory();
        if (globalStack == nullptr) globalStack = linker->getGlobalStack();

        int64_t stack_low  = linker->getTail();
        int64_t stack_high = linker->getTail() + WasmGlobal::getCommonStackSize();

        if (globalStack == nullptr)
        {
            wasm_valtype_t* valtype = wasm_valtype_new(WASM_I64);
            wasm_globaltype_t* gtype = wasm_globaltype_new(valtype, WASM_VAR);

            wasmtime_val_t init_val;
            init_val.kind = WASMTIME_I64;
            init_val.of.i64 = stack_high;

            error = wasmtime_global_new(context, gtype, &init_val, &module->mainThread.globalStack);
            wasm_globaltype_delete(gtype);

            if (WasmTools::logandDeleteWASMFail(error, "Global Stack")) {
                module->destroy();
                return nullptr;
            }

        globalStack = &module->mainThread.globalStack;
        } else
        {
            module->mainThread.globalStack = *globalStack;
        }

        wasmtime_extern_t stack_extern;
        stack_extern.kind = WASMTIME_EXTERN_GLOBAL;
        stack_extern.of.global = *globalStack;

        error = wasmtime_linker_define(
            linker->getLinker(),
            context,
            "env", strlen("env"),
            "__stack_pointer", strlen("__stack_pointer"),
            &stack_extern
        );

        if (WasmTools::logandDeleteWASMFail(error, "Stack Pointer Define")) {
            module->destroy();
            return nullptr;
        }

        LOG_INFO_VERBOSE(logInstance, 2) << "[Linker] Successfully registered shared __stack_pointer at offset: " << stack_high;

        if (shared_memory == nullptr)
        {
            constexpr uint32_t WASM_PAGE_SIZE = 65536;
            wasm_limits_t limits = appMemoryLimits;
            if (limits.min < 256) limits.min = 256;
            // check if limit.max can fit tail
            int64_t tail_bytes = linker->getTail();
            auto required_pages = static_cast<uint32_t>((tail_bytes + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE);

            // 2. Ensure limits.min is at least enough to hold the tail
            if (limits.min < required_pages)
            {
                limits.min = required_pages;
            }

            if (limits.max < required_pages)
            {
                module->destroy();
                return nullptr;
            }

            // 0x2000000 is the 32MB safeguard, we'll need it to fit everything.
            size_t max_bytes = 0x2000000 + static_cast<size_t>(limits.max) * WASM_PAGE_SIZE;

            // We need to put an additional 0x2000000 for the wasmtime safeguard
            linker->setMappedMemory(MappedMemoryFactory::allocate(max_bytes, 0x2000000));
            if (linker->getMainMemoryMapping()->getFileDescriptor() == -1) {
                module->destroy();
                return nullptr;
            }

            wasm_memorytype_t* mem_type;
            wasmtime_memorytype_new(limits.min, true, limits.max, true, true, 16, &mem_type);
            error = wasmtime_sharedmemory_new_with_handle(
                                        engine,
                                        mem_type,
                                        linker->getMainMemoryMapping()->getFileDescriptor(),
                                        &module->sharedMemory
                                    );

            if (WasmTools::logandDeleteWASMFail(error, "Shared Memory Init"))
            {
                module->destroy();
                return nullptr;
            }
            wasm_memorytype_delete(mem_type);

            vec8 buffer2;
            SupervisorCodecs::GeneralFileDescriptor(std::make_unique<SupervisorCodecs::WasmMemoryMappingFileDescriptor>()).serialize(buffer2);
            linker->getSupervisor()->communicate(SendFileDescriptor, buffer2.data(), buffer2.size());
            {
                std::unique_lock<std::mutex> lock(linker->mtx);
                linker->was_filedescriptor_received.wait(lock);
            }
            linker->getSupervisor()->getSocket()->send_fd(linker->getMainMemoryMapping()->getFileDescriptor());
            {
                std::unique_lock<std::mutex> lock(linker->mtx);
                linker->was_filedescriptor_received.wait(lock);
            }

        } else
        {
            module->sharedMemory = shared_memory;
        }

        wasmtime_extern_t mem_extern;
        mem_extern.kind = WASMTIME_EXTERN_SHAREDMEMORY;
        mem_extern.of.sharedmemory = module->sharedMemory;

        wasmtime_linker_define(
            linker->getLinker(),
            context,
            "env", 3,        // Module namespace (e.g., "env")
            "memory", 6,     // Import name
            &mem_extern
        );


        if (linker->getGlobalTable() == nullptr)
        {
            // todo: update c-api to remove this bullshit.
            module->mainThread.sharedTable = WasmTools::createTable(engine, context, linker);
        }
        else
        {
            module->mainThread.sharedTable = *linker->getGlobalTable();
        }

        wasmtime_extern_t table_extern;
        table_extern.kind = WASMTIME_EXTERN_TABLE;
        table_extern.of.table = module->mainThread.sharedTable;

        error = wasmtime_linker_define(
            linker->getLinker(),
            context,
            "env", 3,
            "__indirect_function_table", 25,
            &table_extern
        );

        if (WasmTools::logandDeleteWASMFail(error, "Table Define Error")) {
            module->destroy();
            return nullptr;
        }
        LOG_INFO_VERBOSE(logInstance, 2) << "[Linker] Successfully registered shared table";

        int64_t memoryBaseOffset = linker->get_module_memory_base(appId, dylink.found ? (1 << dylink.mem_align) : 16, memoryDataSize); // e.g., 0 or dynamic heap offset
        int64_t tableBaseOffset  = linker->get_module_table_base(appId, dylink.found ? (1 << dylink.table_align) : 1, slots);  // e.g., 0 or dynamic table index

        {
            wasm_valtype_t* valtype = wasm_valtype_new(WASM_I64);
            wasm_globaltype_t* gtype = wasm_globaltype_new(valtype, WASM_CONST);

            wasmtime_val_t init_val;
            init_val.kind = WASMTIME_I64;
            init_val.of.i64 = memoryBaseOffset;

            error = wasmtime_global_new(context, gtype, &init_val, &module->mainThread.globalMemoryBase);
            wasm_globaltype_delete(gtype);

            if (WasmTools::logandDeleteWASMFail(error, "Memory Base Init")) {
                module->destroy();
                return nullptr;
            }

            wasmtime_extern_t mem_base_extern;
            mem_base_extern.kind = WASMTIME_EXTERN_GLOBAL;
            mem_base_extern.of.global = module->mainThread.globalMemoryBase;

            error = wasmtime_linker_define(
                linker->getLinker(),
                context,
                "env", 3,
                "__memory_base", 13,
                &mem_base_extern
            );

            if (WasmTools::logandDeleteWASMFail(error, "Memory Base Define")) {
                module->destroy();
                return nullptr;
            }
        }

        // 2. Define __table_base (env.__table_base : i64)
        {
            wasm_valtype_t* valtype = wasm_valtype_new(WASM_I64);
            wasm_globaltype_t* gtype = wasm_globaltype_new(valtype, WASM_CONST);

            wasmtime_val_t init_val;
            init_val.kind = WASMTIME_I64;
            init_val.of.i64 = tableBaseOffset;

            error = wasmtime_global_new(context, gtype, &init_val, &module->mainThread.globalTableBase);
            wasm_globaltype_delete(gtype);

            if (WasmTools::logandDeleteWASMFail(error, "Table Base Init")) {
                module->destroy();
                return nullptr;
            }

            wasmtime_extern_t table_base_extern;
            table_base_extern.kind = WASMTIME_EXTERN_GLOBAL;
            table_base_extern.of.global = module->mainThread.globalTableBase;

            error = wasmtime_linker_define(
                linker->getLinker(),
                context,
                "env", 3,
                "__table_base", 12,
                &table_base_extern
            );

            if (WasmTools::logandDeleteWASMFail(error, "Table Base Define")) {
                module->destroy();
                return nullptr;
            }
        }


    module->memoryBaseValue = memoryBaseOffset;
    module->tableBaseValue = tableBaseOffset;


    LOG_INFO_VERBOSE(logInstance, 2) << "[Linker] Dynamic bases set -> __memory_base: "
                                     << module->memoryBaseValue << ", __table_base: " << module->tableBaseValue
                                     << ", __heap_base: " << (linker->getTail() + WasmGlobal::getCommonStackSize()) << ", __stack_low: " << stack_low
                                     << ", __stack_high: " << stack_high;


    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__heap_base", linker->getTail() + WasmGlobal::getCommonStackSize(), true);
    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__stack_low", stack_low, true);
    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__stack_high", stack_high, true);
    wasmtime_context_set_epoch_deadline(context, 0);
    wasm_trap_t *trap = nullptr;
    wasmtime_call_future_t *future = wasmtime_linker_instantiate_async(linker->getLinker(), context, module->module, &module->mainThread.instance, &trap, &error);
    while (!wasmtime_call_future_poll(future)) {
        std::this_thread::yield();
    }

    wasmtime_call_future_delete(future);
    if (WasmTools::logandDeleteWASMFail(error, trap, appId+" Linker Initalize") )
    {
         module->destroy();
         return nullptr;
    }

    wasmtime_func_t func;
    if (WasmTools::getFunctionInInstance(&module->mainThread, "__wasm_apply_data_relocs", &func))
    { // some libraries do not contain data.
        WasmTools::CallWASMFunction<void>(&module->mainThread, module, func, &trap, &error);
        if (WasmTools::logandDeleteWASMFail(error, trap, appId+" Relocation Call"))
        {
            module->destroy();
            return nullptr;
        }
    }

    if (BoraThreadSymbols::threads.empty())
    {
        BoraThreadSymbols::CreateMainThread(module);
    }
    linker->setGlobalSharedMemory(module->sharedMemory);
    linker->setGlobalTable(&module->mainThread.sharedTable);
    linker->setGlobalStackPointer(&module->mainThread.globalStack);

    return module;
}



}

const WasmThreadInstance* WasmRunner::createThreadInstance(WasmGlobal* linker, wasmtime_store_t* store, wasmtime_context_t* context,
   const WasmRuntimeModule* module, u64 stackOffset)
{
    auto logInstance = LogManager::instance().getLogger("bora.thread.module");
    auto* instance = new WasmThreadInstance(module->appId, context, store);
    wasmtime_error_t *error = NULL;
    int64_t stack_low  = stackOffset;
    int64_t stack_high = stack_low + WasmGlobal::getCommonStackSize();

{
    wasm_valtype_t* valtype = wasm_valtype_new(WASM_I64);
    wasm_globaltype_t* gtype = wasm_globaltype_new(valtype, WASM_VAR);

    wasmtime_val_t init_val;
    init_val.kind = WASMTIME_I64;
    init_val.of.i64 = stack_high;

    error = wasmtime_global_new(context, gtype, &init_val, &instance->globalStack);
    wasm_globaltype_delete(gtype);

    if (WasmTools::logandDeleteWASMFail(error, "Global Stack")) {
        delete instance;
        return nullptr;
    }

    wasmtime_extern_t stack_extern;
    stack_extern.kind = WASMTIME_EXTERN_GLOBAL;
    stack_extern.of.global = instance->globalStack;

    error = wasmtime_linker_define(
        linker->getLinker(),
        context,
        "env", strlen("env"),
        "__stack_pointer", strlen("__stack_pointer"),
        &stack_extern
    );

    if (WasmTools::logandDeleteWASMFail(error, "Stack Pointer Define")) {
        delete instance;
        return nullptr;
    }
    LOG_INFO_VERBOSE(logInstance, 2) << "[Linker] Successfully registered shared __stack_pointer at offset: " << stack_high;

        instance->sharedTable = WasmTools::createTable(linker->getEngine(), context, linker);
        wasmtime_extern_t table_extern;
        table_extern.kind = WASMTIME_EXTERN_TABLE;
        table_extern.of.table = instance->sharedTable;
        error = wasmtime_linker_define(
            linker->getLinker(),
            context,
            "env", 3,
            "__indirect_function_table", 25,
            &table_extern
        );
        }
        {
            wasm_valtype_t* valtype = wasm_valtype_new(WASM_I64);
            wasm_globaltype_t* gtype = wasm_globaltype_new(valtype, WASM_CONST);

            wasmtime_val_t init_val;
            init_val.kind = WASMTIME_I64;
            init_val.of.i64 = module->memoryBaseValue;

            error = wasmtime_global_new(context, gtype, &init_val, &instance->globalMemoryBase);
            wasm_globaltype_delete(gtype);

            if (WasmTools::logandDeleteWASMFail(error, "Memory Base Init")) {
                delete instance;
                return nullptr;
            }

            wasmtime_extern_t mem_base_extern;
            mem_base_extern.kind = WASMTIME_EXTERN_GLOBAL;
            mem_base_extern.of.global = instance->globalMemoryBase;

            error = wasmtime_linker_define(
                linker->getLinker(),
                context,
                "env", 3,
                "__memory_base", 13,
                &mem_base_extern
            );

            if (WasmTools::logandDeleteWASMFail(error, "Memory Base Define")) {
                delete instance;
                return nullptr;
            }
        }

        // 2. Define __table_base (env.__table_base : i64)
        {
            wasm_valtype_t* valtype = wasm_valtype_new(WASM_I64);
            wasm_globaltype_t* gtype = wasm_globaltype_new(valtype, WASM_CONST);

            wasmtime_val_t init_val;
            init_val.kind = WASMTIME_I64;
            init_val.of.i64 = module->tableBaseValue;

            error = wasmtime_global_new(context, gtype, &init_val, &instance->globalTableBase);
            wasm_globaltype_delete(gtype);

            if (WasmTools::logandDeleteWASMFail(error, "Table Base Init")) {
                delete instance;
                return nullptr;
            }

            wasmtime_extern_t table_base_extern;
            table_base_extern.kind = WASMTIME_EXTERN_GLOBAL;
            table_base_extern.of.global = instance->globalTableBase;

            error = wasmtime_linker_define(
                linker->getLinker(),
                context,
                "env", 3,
                "__table_base", 12,
                &table_base_extern
            );

            if (WasmTools::logandDeleteWASMFail(error, "Table Base Define")) {
                delete instance;
                return nullptr;
            }
        }

    LOG_INFO_VERBOSE(logInstance, 2) << "[Linker] Dynamic bases set -> __memory_base: "
                                     << module->memoryBaseValue << ", __table_base: " << module->tableBaseValue
                                     << ", __heap_base: " << (linker->getTail() + WasmGlobal::getCommonStackSize())  << ", __stack_low: " << stack_low
                                     << ", __stack_high: " << stack_high;

    if (WasmTools::logandDeleteWASMFail(error, "Thread Table Define Error")) {
        delete instance;
        return nullptr;
    }

    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__heap_base", linker->getTail() + WasmGlobal::getCommonStackSize(), true, context);
    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__stack_low", stack_low, true, context);
    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__stack_high", stack_high, true, context);

    wasm_trap_t *trap = nullptr;
    wasmtime_call_future_t *future = wasmtime_linker_instantiate_async(linker->getLinker(), context, module->module, &instance->instance, &trap, &error);
    while (!wasmtime_call_future_poll(future)) {
        std::this_thread::yield();
    }

    wasmtime_call_future_delete(future);
    if (WasmTools::logandDeleteWASMFail(error, trap, module->appId+" Linker Initalize") )
    {
        delete instance;
         return nullptr;
    }
    
    return instance;
}
