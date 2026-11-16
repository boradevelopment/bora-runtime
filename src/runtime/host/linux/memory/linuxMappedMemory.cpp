#include "linuxMappedMemory.h"

linuxMappedMemory::linuxMappedMemory(int filedescriptor, u64 lOffset): fd(filedescriptor), offset(lOffset) {
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

linuxMappedMemory::linuxMappedMemory(size_t size, bool map, u64 lOffset): memSize(size), offset(lOffset) {
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

linuxMappedMemory::linuxMappedMemory(const u8 *sourceData, size_t size, u64 offset): linuxMappedMemory(size, true, offset) {
    if (sourceData != nullptr && size > 0) {
        std::memcpy(memBase, sourceData, size);
    }
}

linuxMappedMemory::~linuxMappedMemory() {
    cleanup();
}

linuxMappedMemory::linuxMappedMemory(linuxMappedMemory &&other) noexcept: fd(other.fd), memSize(other.memSize), memBase(other.memBase), offset(other.offset) {
    other.fd = -1;
    other.memSize = 0;
    other.memBase = nullptr;
    other.offset = 0;
}

linuxMappedMemory & linuxMappedMemory::operator=(linuxMappedMemory &&other) noexcept {
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

void linuxMappedMemory::setOffset(u64 lOffset) {
    offset = lOffset;
}

sUniquePointer<IMappedMemory> linuxMappedMemory::duplicate() {
    auto memDupe = std::make_unique<linuxMappedMemory>();
    memDupe->fd = dup(fd);
    memDupe->memSize = memSize;
    memDupe->memBase = memBase;
    memDupe->isDupe = true;

    return memDupe;
}

void linuxMappedMemory::cleanup() {
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
