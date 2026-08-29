// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: WasmCacheManager.h
 * Purpose: Manages how WASM modules are cached.
 */
#pragma once
#include <filesystem>
#include <wasm.h>

#include "V2Archive.h"

namespace fs = std::filesystem;

class WasmCacheManager
{
public:
    WasmCacheManager();
    static std::string ComputeWasmChecksum(const std::vector<uint8_t>& wasmBytes);
    static std::string ComputeWasmChecksum(const wasm_byte_vec_t bytes);

    static fs::path GetCacheDirectory();
    static boolean doesCacheExist(const char* id);
    // Helper to sanitize filename fallback if ID variable isn't present
    static std::string GetAppIDOrFallback(const V2Archive& boraApp, const char* tazaFileName);
    static void clearCache();
    static boolean saveBytesToCache(const char* id, const wasm_byte_vec_t bytes);
    static vec8 loadAppFromCache(const char* id);
    static vec8 loadAppFromCacheHash(const char* id, const char* hash);
};
