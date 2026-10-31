// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: linuxMappedMemory.h
 * Purpose: ?
*/
#pragma once
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <sys/stat.h>

#include "IMappedMemory.h"

class linuxMappedMemory : public IMappedMemory {
public:
    linuxMappedMemory() = default;

    linuxMappedMemory(int filedescriptor, u64 lOffset) : fd(filedescriptor), offset(lOffset)
    {
        if (fd > 0)
        {
            struct stat st{};
            if (fstat(fd, &st) < 0) {
                return;
            }
            memSize = st.st_size;


            void* ptr = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (ptr == MAP_FAILED) {
                close(fd);
                throw std::runtime_error("Failed to mmap memory file descriptor");
            }

            memBase = static_cast<u8*>(ptr);
        }
    }

    explicit linuxMappedMemory(size_t size, bool map = true, u64 lOffset = 0) : memSize(size), offset(lOffset) {
        if (memSize == 0) {
            throw std::invalid_argument("Mapped memory size must be greater than zero.");
        }

        // Create an anonymous file descriptor in RAM
        fd = memfd_create("posix_mapped_memory", MFD_CLOEXEC);
        if (fd == -1) {
            throw std::runtime_error("Failed to create memory file descriptor via memfd_create");
        }

        // Size the file descriptor
        if (ftruncate(fd, memSize) == -1) {
            close(fd);
            throw std::runtime_error("Failed to size memory file descriptor via ftruncate");
        }

        // Map the memory
        if (map)
        {
            void* ptr = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (ptr == MAP_FAILED) {
                close(fd);
                throw std::runtime_error("Failed to mmap memory file descriptor");
            }
            
            struct stat st;
            fstat(fd, &st);

            memBase = static_cast<u8*>(ptr);
        }
    }

    // 2. Constructor: Allocate and copy existing u8* data into shared memory
    linuxMappedMemory(const u8* sourceData, size_t size, u64 offset = 0) : linuxMappedMemory(size, true, offset) {
        if (sourceData != nullptr && size > 0) {
            std::memcpy(memBase, sourceData, size);
        }
    }

    // Destructor: Clean up mapping and close file descriptor
    ~linuxMappedMemory() override {
        cleanup();
    }

    // Move semantics (prevent double-free/double-close)
    linuxMappedMemory(linuxMappedMemory&& other) noexcept
        : fd(other.fd), memSize(other.memSize), memBase(other.memBase), offset(other.offset) {
        other.fd = -1;
        other.memSize = 0;
        other.memBase = nullptr;
        other.offset = 0;
    }

    linuxMappedMemory& operator=(linuxMappedMemory&& other) noexcept {
        if (this != &other) {
            cleanup();
            fd = other.fd;
            memSize = other.memSize;
            memBase = other.memBase;
            other.fd = -1;
            other.memSize = 0;
            other.memBase = nullptr;
        }
        return *this;
    }

    // Delete copy semantics
    linuxMappedMemory(const linuxMappedMemory&) = delete;
    linuxMappedMemory& operator=(const linuxMappedMemory&) = delete;

    // Interface overrides
    u8* getData() const override { return memBase + offset; }
    size_t getSize() const override { return memSize; }
    int getFileDescriptor() const override { return fd; }

    void setOffset(u64 lOffset) override
    {
        offset = lOffset;
    };

    sUniquePointer<IMappedMemory> duplicate() override
    {
        auto memDupe = std::make_unique<linuxMappedMemory>();
        memDupe->fd = dup(fd);
        memDupe->memSize = memSize;
        memDupe->memBase = memBase;
        memDupe->isDupe = true;

        return memDupe;
    }

private:
    int fd = -1;
    size_t memSize = 0;
    u8* memBase = nullptr;
    u64 offset = 0;
    bool isDupe = false;

    void cleanup() {
        if (!isDupe)
        {
            if (memBase != nullptr && memBase != MAP_FAILED && memSize > 0) {
                munmap(memBase, memSize);
            }
        }

        if (fd != -1) {
            close(fd);
        }
        fd = -1;
        memSize = 0;
        memBase = nullptr;
        offset = 0;
    }
};
