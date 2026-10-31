// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "MappedMemoryFactory.h"

#include "memory/linuxMappedMemory.h"

std::unique_ptr<IMappedMemory> MappedMemoryFactory::filedescriptor(int fd, u64 offset)
{
#ifdef __linux__
    return std::make_unique<linuxMappedMemory>(fd, offset);
#endif

    return nullptr; // unsupported
}

std::unique_ptr<IMappedMemory> MappedMemoryFactory::create(size_t size, u64 offset)
{
#ifdef __linux__
    return std::make_unique<linuxMappedMemory>(size, false, offset);
#endif

    return nullptr; // unsupported
}

std::unique_ptr<IMappedMemory> MappedMemoryFactory::allocate(size_t size, u64 offset)
{
#ifdef __linux__
    return std::make_unique<linuxMappedMemory>(size, true, offset);
#endif

    return nullptr; // unsupported
}

std::unique_ptr<IMappedMemory> MappedMemoryFactory::copy(u8* data, size_t size, u64 offset)
{
#ifdef __linux__
    return std::make_unique<linuxMappedMemory>(data, size, offset);
#endif

    return nullptr; // unsupported
}
