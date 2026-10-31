// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: IAppPermissions.h
 * Purpose: Holds the data for all possible App Permissions
*/
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "../networking/supervisor/ISupervisorInterface.h"

// App Permissions are more restrictive permissions for native system privileges
// App Permissions are a bit complicated for things like filesystem where a folder will need a permission other than the execution directory and executable directory
// But there are simpler things like bool for doing manual inputs/keystrokes, etc
// I'll add more overtime
struct AppPermissions : ISupervisorCodec {
    // Simpler flags
    bool allowManualInput = false;      // Keystrokes, mouse control
    bool allowNetworkAccess = false;    // Network sockets, HTTP requests
    bool allowSubprocessSpawn = false;  // Executing external system binaries

    // Granular Filesystem paths (beyond the base execution/executable directory)
    sVec<std::string> allowedReadPaths;
    sVec<std::string> allowedWritePaths;

protected:
    // Serializes the permission structure into a binary vector (vec8)
    [[nodiscard]] bool serializeFunc(vec8& buffer) const override {
        VectorWriter writer(buffer);

        // 1. Pack boolean flags
        u8 flags = (allowManualInput ? 1 : 0) |
                        (allowNetworkAccess ? 2 : 0) |
                        (allowSubprocessSpawn ? 4 : 0);
        writer.write(flags);

        // 2. Pack allowed read paths
        u32 readCount = static_cast<u32>(allowedReadPaths.size());
        writer.write(readCount);
        for (const auto& path : allowedReadPaths) {
            u32 pathLen = static_cast<u32>(path.size());
            writer.write(pathLen);
            writer.writeBytes(path.data(), pathLen);
        }

        // 3. Pack allowed write paths
        u32 writeCount = static_cast<u32>(allowedWritePaths.size());
        writer.writeBytes(&writeCount, sizeof(writeCount));
        for (const auto& path : allowedWritePaths) {
            u32 pathLen = static_cast<u32>(path.size());
            writer.write(pathLen);
            writer.writeBytes(path.data(), pathLen);
        }

        return true;
    }

    // Deserializes the permission structure from a binary buffer
    bool deserializeFunc(std::span<const u8> buffer, size_t offset) override {
        BufferReader reader(buffer, offset);

        // 1. Read boolean flags
        u8 flags = 0;
        reader.read(flags);
        allowManualInput = (flags & 1) != 0;
        allowNetworkAccess = (flags & 2) != 0;
        allowSubprocessSpawn = (flags & 4) != 0;

        // 2. Read allowed read paths
        u32 readCount = 0;
        reader.read(readCount);
        allowedReadPaths.clear();
        allowedReadPaths.reserve(readCount);
        for (u32 i = 0; i < readCount; ++i) {
            u32 pathLen = 0;
            if (!reader.read(pathLen)) return false;
            const char* pathString = nullptr;
            reader.readBytes((void*)pathString, pathLen);
            std::string path(pathString, pathLen);
            allowedReadPaths.push_back(std::move(path));
        }

        // 3. Read allowed write paths
        u32 writeCount = 0;
        if (!reader.read(writeCount)) return false;
        allowedWritePaths.clear();
        allowedWritePaths.reserve(writeCount);
        for (u32 i = 0; i < writeCount; ++i) {
            u32 pathLen = 0;
            if (!reader.read(pathLen)) return false;
            const char* pathString = nullptr;
            reader.readBytes((void*)pathString, pathLen);
            std::string path(pathString, pathLen);
            allowedWritePaths.push_back(std::move(path));
        }

        return true;
    }
};
