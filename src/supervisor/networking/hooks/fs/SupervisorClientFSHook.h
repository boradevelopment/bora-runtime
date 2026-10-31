// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: SupervisorClientFSHook.h
 * Purpose: Sets up the hook for the filesystem in clients.
*/
#pragma once
#include <fcntl.h>

#include "networking/supervisor/codecs/FileSystem.h"
#include "networking/supervisor/codecs/Primitives.h"
#include "supervisor/networking/hooks/HookTemplate.h"
#include "networking/supervisor/hooks/SupervisorHookRegister.h"
#include "nThread/ThreadBufferPool.h"

class SupervisorClientFSHook
{
public:
    struct FileEntry {
        int hostFd;
        off_t currentOffset = 0; // Tracks POSIX-compliant file position
    };

    struct SupervisorFileRegistry {
        std::mutex mutex;
        std::atomic<u32> nextHandle{1};
        // Maps global supervisor file handle -> FileEntry struct containing fd and offset
        std::unordered_map<u32, FileEntry> activeFiles;

        u32 registerFile(int hostFd) {
            std::lock_guard<std::mutex> lock(mutex);
            u32 handle = nextHandle++;
            activeFiles[handle] = FileEntry{hostFd, 0};
            return handle;
        }

        void closeFile(u32 handle) {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = activeFiles.find(handle);
            if (it != activeFiles.end()) {
                close(it->second.hostFd);
                activeFiles.erase(it);
            }
        }

        // Helper to safely retrieve the file entry under lock
        bool getFileEntry(u32 handle, FileEntry** outEntry) {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = activeFiles.find(handle);
            if (it != activeFiles.end()) {
                *outEntry = &it->second;
                return true;
            }
            return false;
        }

        // Helper to update the offset (e.g., used for lseek or implicit read advancement)
        void advanceOffset(u32 handle, ssize_t bytesAdvanced) {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = activeFiles.find(handle);
            if (it != activeFiles.end()) {
                it->second.currentOffset += bytesAdvanced;
            }
        }

        // Helper if you implement explicit lseek hooks later
        void setOffset(u32 handle, off_t newOffset) {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = activeFiles.find(handle);
            if (it != activeFiles.end()) {
                it->second.currentOffset = newOffset;
            }
        }
    };

    static SupervisorFileRegistry& getRegistry() {
        static SupervisorFileRegistry registry;
        return registry;
    }

    static void open_file(i64 clientId, const SupervisorMessage& msg, SupervisorSocketManager* managerInstance)
    {
        auto client = SupervisorGlobal::instance().getClient(clientId);
        if (!client) return;
        vec8& buffer = ThreadBufferPool::getScratchBuffer();

        // todo: get permission, for now, don't check just get file
        auto path = SupervisorCodecs::String(msg.payload);
        // auto mode = SupervisorCodecs::String(msg.payload, (sizeof(size_t)+path->size()));
        // open file and register it as a special handle in our hook staticly
        int host_fd = open(path->c_str(), O_RDWR);
        if (host_fd < 0) {         // todo: permission.
            // File not found or permission denied on host
            // Send back a failure response packet with handle = -1
            SupervisorCodecs::File().serialize(buffer);
            managerInstance->reply(msg, clientId, buffer.data(), buffer.size());
            return;
        }

        // 4. Generate a unique supervisor-side handle ID
        u32 supervisorHandle = getRegistry().registerFile(host_fd);
        auto spFile = SupervisorCodecs::File();
        spFile.handle = supervisorHandle;
        spFile.serialize(buffer);
        managerInstance->reply(msg, clientId, buffer.data(), buffer.size());
    }

    static void close_file(i64 clientId, const SupervisorMessage& msg, SupervisorSocketManager* managerInstance)
    {
         auto client = SupervisorGlobal::instance().getClient(clientId);
        if (!client) return;

        if (msg.payload.empty())
        {
            managerInstance->reply(msg, clientId);
            return;
        }

        i32 handle;
        BufferReader(msg.payload).read(handle);
        FileEntry* fileEntry = nullptr;
        auto isValidFile = getRegistry().getFileEntry(handle, &fileEntry);
        if (!isValidFile) {
            managerInstance->reply(msg, clientId);
            return;
        }

        getRegistry().closeFile(handle);
        vec8& buffer = ThreadBufferPool::getScratchBuffer();
        VectorWriter(buffer).write(0);
        managerInstance->reply(msg, clientId, buffer.data(), buffer.size());
    }

    static void seek_file(i64 clientId, const SupervisorMessage& msg, SupervisorSocketManager* managerInstance)
    {
        auto client = SupervisorGlobal::instance().getClient(clientId);
        if (!client) return;

        if (msg.payload.empty())
        {
            managerInstance->reply(msg, clientId);
            return;
        }

        i32 handle, position; // POSITION corresponds to POSIX whence (SEEK_SET, SEEK_CUR, SEEK_END)
        i64 offset;

        auto reader = BufferReader(msg.payload);
        reader.read(handle);
        reader.read(offset);
        reader.read(position);

        FileEntry* fileEntry = nullptr;
        auto isValidFile = getRegistry().getFileEntry(handle, &fileEntry);
        if (!isValidFile) {
            managerInstance->reply(msg, clientId);
            return;
        }

        off_t newOffset = lseek(fileEntry->hostFd, offset, position);

        if (newOffset == static_cast<off_t>(-1)) {
            managerInstance->reply(msg, clientId);
            return;
        }

        getRegistry().setOffset(handle, newOffset);
        vec8& buffer = ThreadBufferPool::getScratchBuffer();
        VectorWriter(buffer).write(newOffset);
        managerInstance->reply(msg, clientId, buffer.data(), buffer.size());
    }

    static void read_file(i64 clientId, const SupervisorMessage& msg, SupervisorSocketManager* managerInstance)
    {
        auto client = SupervisorGlobal::instance().getClient(clientId);
        if (!client) return;

        // Payload will include 4 byte handle and 8 byte wasm offset to put memory in
        auto fileReadInstructions = SupervisorCodecs::ReadFile(msg.payload);
        // find file
        FileEntry* fileEntry = nullptr;
        auto isValidFile = getRegistry().getFileEntry(fileReadInstructions.handle, &fileEntry);
        if (!isValidFile) {
            managerInstance->reply(msg, clientId);
            return;
        }

        u8* shared_mem_base = client->getWasmMemoryMapping()->getData();
        void* target_dest = shared_mem_base + fileReadInstructions.offset;
        size_t totalBytesToRead = fileReadInstructions.size * fileReadInstructions.nmemb;
        ssize_t bytesRead = pread(fileEntry->hostFd, target_dest, totalBytesToRead, fileEntry->currentOffset);
        if (bytesRead > 0) {
            getRegistry().advanceOffset(fileReadInstructions.handle, bytesRead);
        }

        vec8& buffer = ThreadBufferPool::getScratchBuffer();
        VectorWriter(buffer).write(bytesRead);
        managerInstance->reply(msg, clientId, buffer.data(), buffer.size());
    }
};

