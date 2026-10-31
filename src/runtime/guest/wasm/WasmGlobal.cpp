// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "WasmGlobal.h"

#include <ranges>

#include "WasmCacheManager.h"
#include "WasmRegistry.h"
#include "app/IAppPermissions.h"
#include "guest/wasm/WasmTools.h"
#include "guest/stl/thread.h"
#include "logging/LogStream.h"
#include "networking/supervisor/hooks/SupervisorHookRegister.h"

WasmGlobal* WasmGlobal::mgr = nullptr;

WasmGlobal::WasmGlobal(wasmtime_context_t* context, wasm_engine_t* engine, wasmtime_store_t* store,
    fs::path app_dir): context_(context), engine_(engine), store_(store), application_dir_(app_dir)
{
    mgr = this;
    wasmtime_error_t* error;
    globalSymbols = new guestImports();
    globalSymbols->initalizeSymbols();
    linker_ = wasmtime_linker_new(engine);
    wasmtime_linker_allow_shadowing(linker_, true);
    error = wasmtime_linker_define_wasi(linker_);
    globalSymbols->registerSymbol(linker_);
    logger_ = LogManager::instance().getLogger("bora.module.libraries");
    supervisor_ = std::make_unique<WasmSupervisorManager>();
    SupervisorHookRegister::registerClient();
    supervisor_->open();
    while (!supervisor_->isConnected())
    {

    }
}

void WasmGlobal::enableDebugging(const wasm_config_t* config, const char* debugAddress)
{
    isDebugging = true;
    if (WasmCacheManager::doesCacheExist("gdb_debugger"))
    {
        auto error = bwasmtime_gdb_create(config, debugAddress, &debugServer_);
        if (!WasmTools::logandDeleteWASMFail(error, nullptr, "Debug GDB Server Creation"))
        {
            auto data = WasmCacheManager::loadAppFromCache("gdb_debugger");
            error = bwasmtime_gdb_create_deserialized(data.data(), data.size(), config, debugAddress, &debugServer_);
            if (!WasmTools::logandDeleteWASMFail(error, nullptr, "Deserialized GDB Load"))
            {
                error = bwasmtime_gdb_get_engine(debugServer_, &engine_);
                WasmTools::logandDeleteWASMFail(error, nullptr, "GDB Server Engine");
            }
        }
    } else
    {
        auto error = bwasmtime_gdb_create(config, debugAddress, &debugServer_);
        if (!WasmTools::logandDeleteWASMFail(error, nullptr, "Debug Server Creation"))
        {
            bwasmtime_gdb_get_engine(debugServer_, &engine_);
            wasm_byte_vec_t debug_data;
            error = bwasmtime_gdb_serialize_component(debugServer_, &debug_data);
            if (!WasmTools::logandDeleteWASMFail(error, nullptr, "GDB Component Serialization"))
                WasmCacheManager::saveBytesToCache("gdb_debugger", debug_data);
        }
    }
}

bool WasmGlobal::validateImports(const wasmtime_module_t* module,
                                           const std::string& library_name) const
{
    LOG_INFO_VERBOSE(logger_, 2) << "Validating imports for dependency: " << library_name << "...";

    wasm_importtype_vec_t imports;
    wasmtime_module_imports(module, &imports);

    for (size_t i = 0; i < imports.size; ++i) {
        wasm_importtype_t* import_item = imports.data[i];
        const wasm_name_t* module_name = wasm_importtype_module(import_item);
        const wasm_name_t* import_name = wasm_importtype_name(import_item);

        std::string_view mod_str(module_name->data, module_name->size);
        std::string_view name_str(import_name->data, import_name->size);

        if (mod_str == "GOT.mem") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "memory") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "__indirect_function_table") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "__stack_pointer") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "__memory_base") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "__table_base") continue; // registered afterwards.

        wasmtime_extern_t item;
        bool found = wasmtime_linker_get(
            linker_,
            context_,              // or store_ context pointer
            mod_str.data(),      // module name string pointer
            mod_str.length(),
            name_str.data(),
            name_str.length(),
            &item                 // output extern item
        );

        if (!found) {
            LOG_ERROR_VERBOSE(logger_, 2) <<
                "Safety violation in '" << library_name <<
                "': Missing required import '" << mod_str << "::" << name_str << "'!";
            return false;
        }
    }

    wasm_importtype_vec_delete(&imports);
    LOG_INFO_VERBOSE(logger_, 2) << "All imports satisfied for: " << library_name;
    return true;
}

sVec<sString> WasmGlobal::getMissingImports(const wasmtime_module_t* module) const
{
    sVec<sString> missing_libraries;
    std::unordered_set<std::string> unique_missing;

    // 1. Fetch all imports required by the WASM module
    wasm_importtype_vec_t imports;
    wasmtime_module_imports(module, &imports);

    for (size_t i = 0; i < imports.size; ++i) {
        wasm_importtype_t* import_item = imports.data[i];

        // WebAssembly imports are split into: module (namespace/library) and name (field/symbol)
        const wasm_name_t* mod_wasm_name = wasm_importtype_module(import_item);
        const wasm_name_t* field_wasm_name = wasm_importtype_name(import_item);

        std::string_view mod_str(mod_wasm_name->data, mod_wasm_name->size);
        std::string_view name_str(field_wasm_name->data, field_wasm_name->size);

        if (mod_str == "GOT.mem") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "memory") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "__indirect_function_table") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "__stack_pointer") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "__memory_base") continue; // registered afterwards.
        if (mod_str == "env" && name_str == "__table_base") continue; // registered afterwards.

        // 2. Check if this import is already present in the Linker
        wasmtime_extern_t item;
        bool exists = wasmtime_linker_get(
            linker_,
            context_,           // store context pointer
            mod_str.data(),
            mod_str.length(),
            name_str.data(),
            name_str.length(),
            &item
        );

        // 3. If missing, extract the library/namespace name
        if (!exists) {
            std::string_view lib_name_sv = mod_str;

            // Optional: If your namespace contains a delimiter like "::" or "."
            // (e.g., "bora_math::core" -> "bora_math"), trim past the delimiter:
            size_t delim_pos = mod_str.find("::");
            if (delim_pos != std::string_view::npos) {
                lib_name_sv = mod_str.substr(0, delim_pos);
            }

            std::string lib_name(lib_name_sv);

            // Avoid duplicate entries in the returned list
            if (unique_missing.insert(lib_name).second) {
                missing_libraries.push_back(sString(lib_name.c_str()));
            }
        }
    }

    // 4. Free vector allocated by wasmtime_module_imports
    wasm_importtype_vec_delete(&imports);

    return missing_libraries;
}

const WasmThreadInstance* WasmGlobal::registerRuntimeLibraryForThread(wasmtime_store_t* store, wasmtime_context_t* context, const WasmRuntimeModule* dependencyModule, u64 stackOffset)
{
    const WasmThreadInstance* instanceDependency = WasmRunner::createThreadInstance(getInstance(), store, context, dependencyModule, stackOffset);

    // wasmtime_func_t func;
    // if (!WasmTools::getFunctionInInstance(instanceDependency, "_initialize", &func))
    // {
    //     instanceDependency->destroy();
    //     return nullptr; //!?
    // }
    wasmtime_error_t* error = nullptr;
    wasm_trap_t* trap = nullptr;
        // WasmTools::CallWASMFunction<void>(instanceDependency, dependencyModule, func, &trap, &error);
        // if (WasmTools::logandDeleteWASMFail(error, trap, dependencyModule->appId+" Initalization"))
        // {
        //     instanceDependency->destroy();
        //     return nullptr;
        // }

    size_t export_index = 0;
    char* orig_name_ptr = nullptr;
    size_t orig_name_len = 0;
    wasmtime_extern_t export_item;

    while (wasmtime_instance_export_nth(context, &instanceDependency->instance, export_index++, &orig_name_ptr, &orig_name_len, &export_item)) {
        std::string export_name(orig_name_ptr, orig_name_len);
        // Replace all occurrences of '$' with "::" since in the official BORA SDK, you use $ for namespaces.
        size_t pos = 0;
        while ((pos = export_name.find('$', pos)) != std::string::npos) {
            export_name.replace(pos, 1, "::");
            pos += 2; // Advance past the inserted "::"
        }
        error = wasmtime_linker_define(
            linker_,
            context,
            dependencyModule->appId.data(),
            dependencyModule->appId.length(),
            export_name.data(),
            export_name.length(),
            &export_item
        );

        wasmtime_extern_delete(&export_item);
        if (error != nullptr) {
            LOG_ERROR_VERBOSE(logger_, 1) << "Failed to define export '" << export_name
                                          << "' in Linker for library: " << dependencyModule->appId;
            wasmtime_error_delete(error);
            return nullptr;
        }
    }

    return instanceDependency;
}

bool WasmGlobal::registerRuntimeLibrary(const std::string& lib_name, const std::string& path, bool skipReqDeps)
{
    if (loaded_libraries_.empty() && !skipReqDeps)
    {
        // if (!registerRuntimeLibrary("syslib", findLibrary("syslib").generic_string(), true))
        // {
        //     return false;
        // }
    }

    if (loaded_libraries_.contains(lib_name)) {
        LOG_WARN_VERBOSE(logger_, 1) << "[Linker] Dynamic library '" << lib_name << "' is already registered.\n";
        return true;
    }

    // Load modules will recursively load needed dependencies if found, so this will be called again.
    const WasmRuntimeModule* dep_module = WasmRunner::loadModuleByTAZA(this, store_, context_, engine_, path.c_str(), sharedMemory_, globalStack_, true);
    if (dep_module == nullptr) return false;

    // Verify all imports needed by this dynamic library are already available
    if (!validateImports(dep_module->module, lib_name))
    {
        dep_module->destroy();
        return false; // We cannot continue if it has a dependency that is unsatisified.
    }

    wasmtime_func_t func;
    if (!WasmTools::getFunctionInInstance(&dep_module->mainThread, "_initialize", &func))
    {
        dep_module->destroy();
        return false; //!?
    }
    wasmtime_error_t* error = nullptr;
    wasm_trap_t* trap = nullptr;
    WasmTools::CallWASMFunction<void>(&dep_module->mainThread, dep_module, func, &trap, &error);
    if (WasmTools::logandDeleteWASMFail(error, trap, lib_name+" Initalization"))
    {
        dep_module->destroy();
        return false;
    }

    size_t export_index = 0;
    char* orig_name_ptr = nullptr;
    size_t orig_name_len = 0;
    wasmtime_extern_t export_item;

    while (wasmtime_instance_export_nth(context_, &dep_module->mainThread.instance, export_index++, &orig_name_ptr, &orig_name_len, &export_item)) {
        std::string export_name(orig_name_ptr, orig_name_len);

        // Replace all occurrences of '$' with "::"
        size_t pos = 0;
        while ((pos = export_name.find('$', pos)) != std::string::npos) {
            export_name.replace(pos, 1, "::");
            pos += 2; // Advance past the inserted "::"
        }

        error = wasmtime_linker_define(
            linker_,
            context_,
            lib_name.data(),
            lib_name.length(),
            export_name.data(),
            export_name.length(),
            &export_item
        );
        
        wasmtime_extern_delete(&export_item);
        if (error != nullptr) {
            LOG_ERROR_VERBOSE(logger_, 1) << "Failed to define export '" << export_name
                                          << "' in Linker for library: " << lib_name;
            wasmtime_error_delete(error);
            return false;
        }
    }
    // Store references to maintain lifetime
    loaded_libraries_.insert(lib_name);
    library_instances_.emplace(lib_name, dep_module);


    LOG_INFO_VERBOSE(logger_, 1) << "[Linker] Successfully registered dynamic library: " << lib_name;
    return true;
}

fs::path WasmGlobal::findLibrary(const std::string& libraryName) const
{
    // Build list of candidate search paths in resolution priority order:
    // Priority 1: Application / .taza execution directory
    // Priority 2: User local library directory
    // Priority 3: System-wide runtime executable directory
    std::vector<fs::path> searchDirectories = {
        application_dir_,
        GetUserLibraryDirectory(),
        GetSystemRuntimeDirectory() / "libraries",
        GetSystemRuntimeDirectory()
    };


    for (const auto& dir : searchDirectories) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) continue;

        // Direct check if libraryName already has extension
        fs::path directPath = dir / libraryName;
        if (fs::exists(directPath) && fs::is_regular_file(directPath)) {
            return fs::canonical(directPath);
        }

        fs::path targetPath = dir / (libraryName + ".brdep");
        if (fs::exists(targetPath) && fs::is_regular_file(targetPath)) {
            return fs::canonical(targetPath);
        }
    }

    // Return empty path if not found anywhere
    return {};
}

int64_t WasmGlobal::get_module_memory_base(const std::string& appId, uint32_t required_alignment, int64_t estimated_data_size)
{
    std::lock_guard<std::mutex> lock(linker_mutex_);

    // 1. If this appId/module was already assigned a memory base, return it
    auto it = module_memory_bases_.find(appId);
    if (it != module_memory_bases_.end()) {
        return it->second;
    }

    int64_t allocated_base = align_to(current_memory_offset_, required_alignment);
    module_memory_bases_[appId] = allocated_base;

    int64_t padded_size = align_to(estimated_data_size, 16);
    current_memory_offset_ = allocated_base + padded_size;

    return allocated_base;
}

int64_t WasmGlobal::get_module_table_base(const std::string& appId, uint32_t table_alignment, int64_t estimated_table_slots)
{
    std::lock_guard<std::mutex> lock(linker_mutex_);
    auto it = module_table_bases_.find(appId);
    if (it != module_table_bases_.end()) {
        return it->second;
    }

    int64_t allocated_base = current_table_offset_;
    module_table_bases_[appId] = allocated_base;

    int64_t padded_size = align_to(estimated_table_slots, table_alignment);
    current_table_offset_ += padded_size;

    return allocated_base;
}


void WasmGlobal::addOnTail(u64 memorySize)
{
    uint32_t alignmentExp = 4;
    m_currentMemoryTail = AlignUp(m_currentMemoryTail   , alignmentExp);
    m_currentMemoryTail += memorySize;
}

void WasmGlobal::removeFromTail(u64 memorySize)
{
    uint32_t alignmentExp = 4;
    m_currentMemoryTail = AlignUp(m_currentMemoryTail   , alignmentExp);
    m_currentMemoryTail -= memorySize;
}

void WasmGlobal::destroy()
{
    if (globalSymbols) delete globalSymbols;
    for (const auto& libInstance : library_instances_) {
        wasmtime_store_epoch_deadline_callback(libInstance.second->mainThread.store, BoraThreadSymbols::stub_epoch_callback, nullptr, nullptr); // fix for epoch callback bugs
        auto instance = libInstance.second;
        auto name = libInstance.first;
        wasmtime_func_t func;
        if (WasmTools::getFunctionInInstance(&instance->mainThread, "__call_dtors", &func))
        {
            wasmtime_error_t* error = nullptr;
            wasm_trap_t* trap = nullptr;
            WasmTools::CallWASMFunction<void>(&instance->mainThread, instance, func, &trap, &error);
            if (!WasmTools::logandDeleteWASMFail(error, trap, name+" Exit"))
            {
                LOG_INFO_VERBOSE(logger_, 3) << name << " Exit Call Successful";
            }
        }
        instance->destroy();
    }
    library_instances_.clear();
    loaded_libraries_.clear();
    module_memory_bases_.clear();
    module_table_bases_.clear();
    current_memory_offset_ = 0;
    current_table_offset_ = 0;
    m_currentMemoryTail = 0;
    if (linker_) wasmtime_linker_delete(linker_);
    if (sharedMemory_) wasmtime_sharedmemory_delete(sharedMemory_);
}

std::list<const WasmRuntimeModule*> WasmGlobal::getLibraries()
{
    std::list<const WasmRuntimeModule*> libraries;

    for (const auto* module : library_instances_ | std::views::values) {
        libraries.push_back(module);
    }

    return libraries;
}

void WasmGlobal::setTail(i64 tail)
{
    m_currentMemoryTail = tail;
}




