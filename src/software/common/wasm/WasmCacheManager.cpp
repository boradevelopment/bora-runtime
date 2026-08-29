// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

#include "WasmCacheManager.h"
#include <iomanip>
#include <ios>
#include <sstream>

WasmCacheManager::WasmCacheManager()
{
}

std::string WasmCacheManager::ComputeWasmChecksum(const std::vector<uint8_t>& wasmBytes)
{
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    for (uint8_t byte : wasmBytes) {
        hash ^= byte;
        hash *= 1099511628211ULL; // FNV prime
    }

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

std::string WasmCacheManager::ComputeWasmChecksum(const wasm_byte_vec_t bytes)
{
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    for (size_t i = 0; i < bytes.size; ++i) {
        uint8_t byte = static_cast<uint8_t>(bytes.data[i]);
        hash ^= byte;
        hash *= 1099511628211ULL; // FNV prime
    }

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

fs::path WasmCacheManager::GetCacheDirectory()
{
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

boolean WasmCacheManager::doesCacheExist(const char* id)
{
    fs::path cacheDir = GetCacheDirectory() / id;
    return fs::exists(cacheDir);
}

std::string WasmCacheManager::GetAppIDOrFallback(const V2Archive& boraApp, const char* tazaFileName)
{
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

void WasmCacheManager::clearCache()
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(GetCacheDirectory(), ec)) {
        fs::remove_all(entry.path(), ec);
    }
}

boolean WasmCacheManager::saveBytesToCache(const char* id, const wasm_byte_vec_t bytes)
{
    sString hash = ComputeWasmChecksum(bytes);
    fs::path cacheDir = GetCacheDirectory() / id;
    fs::create_directories(cacheDir);

    fs::path filePath = cacheDir / hash;
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    file.write(bytes.data, bytes.size);
    file.close();

    return true;
}

vec8 WasmCacheManager::loadAppFromCache(const char* id)
{
    fs::path cacheDir = GetCacheDirectory() / id;

    std::error_code ec;
    if (!fs::exists(cacheDir, ec) || !fs::is_directory(cacheDir, ec)) {
        return vec8();
    }

    std::optional<fs::path> mostRecentFile;
    fs::file_time_type maxTime;

    // Iterate through files to find the latest last_write_time
    for (const auto& entry : fs::directory_iterator(cacheDir, ec)) {
        if (entry.is_regular_file(ec)) {
            auto writeTime = entry.last_write_time(ec);
            if (!ec) {
                if (!mostRecentFile || writeTime > maxTime) {
                    maxTime = writeTime;
                    mostRecentFile = entry.path();
                }
            }
        }
    }

    if (!mostRecentFile) {
        return vec8(); // No valid files found in cache directory
    }

    // Read the binary data into memory (e.g. for deserialization)
    std::ifstream file(*mostRecentFile, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return vec8();
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    vec8 buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        return buffer;
    }

    return vec8();
}
