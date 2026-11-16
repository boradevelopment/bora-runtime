// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: darwinMappedMemory.h
 * Purpose: ?
*/
#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>
#include <random>
#include <sys/mman.h>
#include "IMappedMemory.h"

class darwinMappedMemory : public IMappedMemory {
public:
    darwinMappedMemory();

    // Constructor: Wrap an existing file descriptor
    darwinMappedMemory(int filedescriptor, u64 lOffset);

    // Constructor: Allocate anonymous/shared memory region (replaces memfd_create)
    explicit darwinMappedMemory(size_t size, bool map = true, u64 lOffset = 0);

    // 3. Constructor: Allocate and copy existing u8* data
    darwinMappedMemory(const u8* sourceData, size_t size, u64 offset = 0);

    // Destructor
    ~darwinMappedMemory() override;

    // Move semantics
    darwinMappedMemory(darwinMappedMemory&& other) noexcept;

    darwinMappedMemory& operator=(darwinMappedMemory&& other) noexcept;

    // Delete copy semantics
    darwinMappedMemory(const darwinMappedMemory&) = delete;
    darwinMappedMemory& operator=(const darwinMappedMemory&) = delete;

    // Interface overrides
    u8* getData() const override;

    size_t getSize() const override;

    int getFileDescriptor() const override;

    void setOffset(u64 lOffset) override;

    sUniquePointer<IMappedMemory> duplicate() override;

private:
    int fd = -1;
    size_t memSize = 0;
    u8* memBase = nullptr;
    u64 offset = 0;
    bool isDupe = false;
    bool ownsFd = true;

    static std::string generateUniqueShmName();

    void cleanup();
};