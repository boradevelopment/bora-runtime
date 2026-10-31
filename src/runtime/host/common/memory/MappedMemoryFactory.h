// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: MappedMemoryFactory.h
 * Purpose: ?
*/
#pragma once
#include "IMappedMemory.h"

class MappedMemoryFactory
{
public:
    // Create's a plain file descriptor with no map.
    static std::unique_ptr<IMappedMemory> filedescriptor(int fd, u64 offset = 0);
    static std::unique_ptr<IMappedMemory> create(size_t size, u64 offset = 0);
    static std::unique_ptr<IMappedMemory> allocate(size_t size, u64 offset = 0);
    static std::unique_ptr<IMappedMemory> copy(u8* data, size_t size, u64 offset = 0);
};

