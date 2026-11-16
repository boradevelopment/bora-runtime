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

    linuxMappedMemory(int filedescriptor, u64 lOffset);

    explicit linuxMappedMemory(size_t size, bool map = true, u64 lOffset = 0);

    // 2. Constructor: Allocate and copy existing u8* data into shared memory
    linuxMappedMemory(const u8* sourceData, size_t size, u64 offset = 0);

    // Destructor: Clean up mapping and close file descriptor
    ~linuxMappedMemory() override;

    // Move semantics (prevent double-free/double-close)
    linuxMappedMemory(linuxMappedMemory&& other) noexcept;

    linuxMappedMemory& operator=(linuxMappedMemory&& other) noexcept;

    // Delete copy semantics
    linuxMappedMemory(const linuxMappedMemory&) = delete;
    linuxMappedMemory& operator=(const linuxMappedMemory&) = delete;

    // Interface overrides
    u8* getData() const override { return memBase + offset; }
    size_t getSize() const override { return memSize; }
    int getFileDescriptor() const override { return fd; }

    void setOffset(u64 lOffset) override;;

    sUniquePointer<IMappedMemory> duplicate() override;

private:
    int fd = -1;
    size_t memSize = 0;
    u8* memBase = nullptr;
    u64 offset = 0;
    bool isDupe = false;

    void cleanup();
};
