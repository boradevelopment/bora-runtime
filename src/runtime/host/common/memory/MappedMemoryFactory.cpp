// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "MappedMemoryFactory.h"
#ifdef __linux__
#include "memory/linuxMappedMemory.h"
#elif defined(__DARWIN__)
#include "memory/darwinMappedMemory.h"
#endif

std::unique_ptr<IMappedMemory> MappedMemoryFactory::filedescriptor(int fd, u64 offset)
{
#ifdef __linux__
    return std::make_unique<linuxMappedMemory>(fd, offset);
#elif defined(__DARWIN__)
    return std::make_unique<darwinMappedMemory>(fd, offset);
#endif

    return nullptr; // unsupported
}

std::unique_ptr<IMappedMemory> MappedMemoryFactory::create(size_t size, u64 offset)
{
#ifdef __linux__
    return std::make_unique<linuxMappedMemory>(size, false, offset);
#elif defined(__DARWIN__)
    return std::make_unique<darwinMappedMemory>(size,false, offset);
#endif

    return nullptr; // unsupported
}

std::unique_ptr<IMappedMemory> MappedMemoryFactory::allocate(size_t size, u64 offset)
{
#ifdef __linux__
    return std::make_unique<linuxMappedMemory>(size, true, offset);
#elif defined(__DARWIN__)
    return std::make_unique<darwinMappedMemory>(size ,true, offset);
#endif

    return nullptr; // unsupported
}

std::unique_ptr<IMappedMemory> MappedMemoryFactory::copy(u8* data, size_t size, u64 offset)
{
#ifdef __linux__
    return std::make_unique<linuxMappedMemory>(data, size, offset);
#elif defined(__DARWIN__)
    return std::make_unique<darwinMappedMemory>(data, size, offset);
#endif

    return nullptr; // unsupported
}
