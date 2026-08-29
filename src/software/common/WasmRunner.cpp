// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

#include "WasmRunner.h"

#include <cstring>

#include "host/WasmTools.h"
#include "logging/Logger.h"
#include "logging/LogManager.h"
#include "logging/LogStream.h"
#include "tools/CPUInfo.h"
#include "wasm/WasmLibraryLinker.h"

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

static bool SaveCWASMToCache(wasmtime_module_t* module, const fs::path& cachePath) {
    wasm_byte_vec_t serialized_bytes;
    wasmtime_error_t* error = wasmtime_module_serialize(module, &serialized_bytes);

    if (error != nullptr) {
        wasmtime_error_delete(error);
        return false;
    }

    std::ofstream outFile(cachePath, std::ios::binary | std::ios::out);
    if (!outFile.is_open()) {
        wasm_byte_vec_delete(&serialized_bytes);
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(serialized_bytes.data), serialized_bytes.size);
    outFile.close();

    wasm_byte_vec_delete(&serialized_bytes);
    return true;
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
    // todo: get the bora app id, use that to cache the CWASM.
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

    if (error != nullptr) {
        wasmtime_error_delete(error);
        return nullptr;
    }

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
                    // Skip init expression (usually i32.const / global.get + end)
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

                // Active segments specify a memory index / init expression
                if ((flags & 1) == 0) {
                    // Active segment: Skip init expression (e.g., i32.const / global.get + 0x0B)
                    while (ptr < sectionEnd && *ptr != 0x0B) ptr++;
                    if (ptr < sectionEnd) ptr++; // Skip 0x0B (end opcode)
                } else if (flags == 1) {
                    // Passive segment (no offset expression, used by memory.init)
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

const WasmRuntimeModule* WasmRunner::loadModuleByTAZA(WasmLibraryLinker* linker, wasmtime_context_t* context, wasm_engine_t* engine,
                                                      const char* tazaFileName, const wasmtime_memory_t* shared_memory, const wasmtime_global_t* globalStack)
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
        std::vector<uint8_t> vData = boraApp.header.getV2File(
            boraAppInputStream,
            entryV2File->second,
            boraApp.iv,
            boraApp.key
        );
        if (vData.empty())
        {
            LOG_ERROR_VERBOSE(logInstance, 1) << "Entry point data has nothing.";
            return nullptr; // what are we going to do nothing?
        }

        std::string appId = ExtractAppIDOrFallback(boraApp, tazaFileName);
        std::string checksum = ComputeWasmChecksum(vData);
        fs::path cacheDir = GetBoraCacheDirectory();
        std::error_code ec;
        fs::create_directories(cacheDir / appId, ec);
        fs::path cwasmCachePath = cacheDir / appId / (checksum);
        // if (linker->getDebugging())
        // {
        //     bwasmtime_gdb_get_engine(linker->getDebugServer(), engine);
        // }

        WasmRuntimeModule* module = new WasmRuntimeModule();
        module->module = nullptr;
        if (fs::exists(cwasmCachePath) && !AppParam::has("debug"))
        {
            LOG_INFO_VERBOSE(logInstance, 2) << "Cache file was found! App ID: " << appId << " Checksum: " << checksum;
            wasm_byte_vec_t cache_bytes;
            if (read_file(cwasmCachePath.string().c_str(), &cache_bytes)) {
                module->module = CreateModuleFromAOTMemory(
                    engine,
                    reinterpret_cast<u8*>(cache_bytes.data),
                    cache_bytes.size
                );
                wasm_byte_vec_delete(&cache_bytes);
            }

            // If deserialization failed (e.g. corrupted cache file), delete it so we recompile
            if (module->module == nullptr) {
                LOG_WARN_VERBOSE(logInstance, 1) << "Cache file is invalid, WASM will be reserialized.";
                std::error_code ec;
                fs::remove(cwasmCachePath, ec);
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


            if (linker->getDebugging()) SaveCWASMToCache(module->module, cwasmCachePath);
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
        linker->add_to_tail(memoryDataSize);
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

        int64_t stack_low  = 0;
        int64_t stack_high = stack_low + (1 * 1024 * 1024);

        if (globalStack == nullptr)
        {
            wasm_valtype_t* valtype = wasm_valtype_new(WASM_I64);
            wasm_globaltype_t* gtype = wasm_globaltype_new(valtype, WASM_VAR);

            wasmtime_val_t init_val;
            init_val.kind = WASMTIME_I64;
            init_val.of.i64 = stack_high;

            error = wasmtime_global_new(context, gtype, &init_val, &module->globalStack);
            wasm_globaltype_delete(gtype);

            if (WasmTools::logandDeleteWASMFail(error, "Global Stack")) {
                module->destroy();
                return nullptr;
            }


            globalStack = &module->globalStack;
        } else
        {
            module->globalStack = *globalStack;
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

            wasm_memorytype_t* mem_type;
            wasmtime_memorytype_new(limits.min, true, limits.max, true, false, 16, &mem_type);
            // wasmtime_sharedmemory_t* shared_mem = nullptr; -- todo: multithreading.
            // error = wasmtime_sharedmemory_new(engine, mem_type, &shared_mem);
            error = wasmtime_memory_new(context, mem_type, &module->sharedMemory);
            if (WasmTools::logandDeleteWASMFail(error, "Shared Memory Init"))
            {
                module->destroy();
                return nullptr;
            }
            wasm_memorytype_delete(mem_type);
        } else
        {
            module->sharedMemory = *shared_memory;
        }

        wasmtime_extern_t mem_extern;
        mem_extern.kind = WASMTIME_EXTERN_MEMORY;
        mem_extern.of.memory = module->sharedMemory;

        wasmtime_linker_define(
            linker->getLinker(),
            context,
            "env", 3,        // Module namespace (e.g., "env")
            "memory", 6,     // Import name
            &mem_extern
        );


        if (linker->getGlobalTable() == nullptr)
        {
            const char* wat_table64_src = R"(
        (module
            (table (export "__indirect_function_table") i64 4096 funcref)
        )
    )";

            // 1. Convert WAT string to WASM bytecode
            wasm_byte_vec_t wasm_bytes;
            wasmtime_error_t* error = wasmtime_wat2wasm(wat_table64_src, std::strlen(wat_table64_src), &wasm_bytes);
            if (WasmTools::logandDeleteWASMFail(error, "Table Creation Bytes")) {
                module->destroy();
                return nullptr;
            }

            wasmtime_module_t* table_module;
            error = wasmtime_module_new(engine, reinterpret_cast<uint8_t*>(wasm_bytes.data), wasm_bytes.size, &table_module);
            wasm_byte_vec_delete(&wasm_bytes);

            if (WasmTools::logandDeleteWASMFail(error, "Table Creation Module")) {
                module->destroy();
                return nullptr;
            }

            wasm_trap_t* trap = nullptr;
            wasmtime_instance_t table_instance;
            wasmtime_call_future_t* future = wasmtime_linker_instantiate_async(linker->getLinker(), context, table_module, &table_instance, &trap, &error);
            while (!wasmtime_call_future_poll(future)) {
                std::this_thread::yield();
            }
            wasmtime_module_delete(table_module);

            if (WasmTools::logandDeleteWASMFail(error, trap, "Table Creation Instance")) {
                module->destroy();
                return nullptr;
            }

            // 4. Extract the 64-bit table from the instance export
            wasmtime_extern_t exported_table;
            bool found = wasmtime_instance_export_get(
                context,
                &table_instance,
                "__indirect_function_table", 25,
                &exported_table
            );

            if (!found || exported_table.kind != WASMTIME_EXTERN_TABLE) {
                module->destroy();
                LOG_ERROR_VERBOSE(logInstance, 1) << "Failed to extract Table64 export from helper module!";
                return nullptr;
            }

            module->sharedTable = exported_table.of.table;
        }
        else
        {
            module->sharedTable = *linker->getGlobalTable();
        }

        wasmtime_extern_t table_extern;
        table_extern.kind = WASMTIME_EXTERN_TABLE;
        table_extern.of.table = module->sharedTable;

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

            error = wasmtime_global_new(context, gtype, &init_val, &module->globalMemoryBase);
            wasm_globaltype_delete(gtype);

            if (WasmTools::logandDeleteWASMFail(error, "Memory Base Init")) {
                module->destroy();
                return nullptr;
            }

            wasmtime_extern_t mem_base_extern;
            mem_base_extern.kind = WASMTIME_EXTERN_GLOBAL;
            mem_base_extern.of.global = module->globalMemoryBase;

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

            error = wasmtime_global_new(context, gtype, &init_val, &module->globalTableBase);
            wasm_globaltype_delete(gtype);

            if (WasmTools::logandDeleteWASMFail(error, "Table Base Init")) {
                module->destroy();
                return nullptr;
            }

            wasmtime_extern_t table_base_extern;
            table_base_extern.kind = WASMTIME_EXTERN_GLOBAL;
            table_base_extern.of.global = module->globalTableBase;

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

    LOG_INFO_VERBOSE(logInstance, 2) << "[Linker] Dynamic bases set -> __memory_base: "
                                     << memoryBaseOffset << ", __table_base: " << tableBaseOffset;

    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__heap_base", linker->getTail(), true);
    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__stack_low", stack_low, true);
    LinkerTemplate::GlobalI64(linker->getLinker(), "GOT.mem", "__stack_high", stack_high, true);

    wasm_trap_t *trap = nullptr;
    wasmtime_call_future_t *future = wasmtime_linker_instantiate_async(linker->getLinker(), context, module->module, &module->instance, &trap, &error);
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
    if (WasmTools::getFunctionInInstance(context, module, "__wasm_apply_data_relocs", &func))
    { // some libraries do not contain data.
        WasmTools::CallWASMFunction<void>(context, module, func, &trap, &error);
        if (WasmTools::logandDeleteWASMFail(error, trap, appId+" Relocation Call"))
        {
            module->destroy();
            return nullptr;
        }

    }

    linker->setGlobalSharedMemory(&module->sharedMemory);
    linker->setGlobalTable(&module->sharedTable);
    linker->setGlobalStackPointer(&module->globalStack);

    return module;
}



}
