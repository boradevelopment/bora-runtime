// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: WasmCacheManager.h
 * Purpose: Manages how WASM modules are cached.
 */
#pragma once
#include <filesystem>
#include <wasm.h>
#include <wasmtime/module.h>

#include "V2Archive.h"

namespace fs = std::filesystem;

class WasmCacheManager
{
public:
    WasmCacheManager();
    static std::string ComputeWasmChecksum(const std::vector<uint8_t>& wasmBytes);
    static std::string ComputeWasmChecksum(wasm_byte_vec_t bytes);

    static fs::path GetCacheDirectory();
    static bool doesCacheExist(const char* id);
    static bool doesCacheExist(const char* id, const char* hash);
    // Helper to sanitize filename fallback if ID variable isn't present
    static std::string GetAppIDOrFallback(const V2Archive& boraApp, const char* tazaFileName);
    static void clearCache();
    static bool saveBytesToCache(const char* id, wasm_byte_vec_t bytes);
    static bool saveBytesToCache(const char* id, sVec<u8>* bytes);
    static bool saveBytesToCache(const char* id, sVec<u8>* originalBytes, wasmtime_module_t* module);

    static bool invalidateAppHash(const char* id, const char* hash);

    static vec8 loadAppFromCache(const char* id);
    static vec8 loadAppFromCache(const char* id, const char* hash);
};
