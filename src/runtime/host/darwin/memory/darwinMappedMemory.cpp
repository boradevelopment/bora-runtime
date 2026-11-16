// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "darwinMappedMemory.h"

darwinMappedMemory::darwinMappedMemory() = default;

darwinMappedMemory::darwinMappedMemory(int filedescriptor, u64 lOffset): fd(filedescriptor), offset(lOffset), ownsFd(false) {
    if (fd >= 0)
    {
        struct stat st{};
        if (fstat(fd, &st) < 0) {
            return;
        }
        memSize = st.st_size;

        void* ptr = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Failed to mmap memory file descriptor on macOS");
        }

        memBase = static_cast<u8*>(ptr);
    }
}

darwinMappedMemory::darwinMappedMemory(size_t size, bool map, u64 lOffset): memSize(size), offset(lOffset), ownsFd(true) {
    if (memSize == 0) {
        throw std::invalid_argument("Mapped memory size must be greater than zero.");
    }

    // Generate a unique name for POSIX shared memory object on macOS
    std::string shmName = generateUniqueShmName();

    // O_RDWR | O_CREAT | O_EXCL with standard POSIX permissions
    fd = shm_open(shmName.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd == -1) {
        throw std::runtime_error("Failed to create POSIX shared memory object via shm_open");
    }

    // Unlink immediately so the name is released; the file descriptor keeps it alive
    shm_unlink(shmName.c_str());

    // Size the shared memory object
    if (ftruncate(fd, memSize) == -1) {
        close(fd);
        throw std::runtime_error("Failed to size memory via ftruncate");
    }

    // Map the memory
    if (map)
    {
        void* ptr = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Failed to mmap shared memory descriptor");
        }

        memBase = static_cast<u8*>(ptr);
    }
}

darwinMappedMemory::darwinMappedMemory(const u8 *sourceData, size_t size, u64 offset): darwinMappedMemory(size, true, offset) {
    if (sourceData != nullptr && size > 0 && memBase != nullptr) {
        std::memcpy(memBase, sourceData, size);
    }
}

darwinMappedMemory::~darwinMappedMemory() {
    cleanup();
}

darwinMappedMemory::darwinMappedMemory(darwinMappedMemory &&other) noexcept: fd(other.fd), memSize(other.memSize), memBase(other.memBase),
                                                                             offset(other.offset), isDupe(other.isDupe), ownsFd(other.ownsFd) {
    other.fd = -1;
    other.memSize = 0;
    other.memBase = nullptr;
    other.offset = 0;
    other.ownsFd = false;
}

darwinMappedMemory & darwinMappedMemory::operator=(darwinMappedMemory &&other) noexcept {
    if (this != &other) {
        cleanup();
        fd = other.fd;
        memSize = other.memSize;
        memBase = other.memBase;
        offset = other.offset;
        isDupe = other.isDupe;
        ownsFd = other.ownsFd;

        other.fd = -1;
        other.memSize = 0;
        other.memBase = nullptr;
        other.ownsFd = false;
    }
    return *this;
}

u8 * darwinMappedMemory::getData() const { return memBase ? (memBase + offset) : nullptr; }

size_t darwinMappedMemory::getSize() const { return memSize; }

int darwinMappedMemory::getFileDescriptor() const { return fd; }

void darwinMappedMemory::setOffset(u64 lOffset) {
    offset = lOffset;
}

sUniquePointer<IMappedMemory> darwinMappedMemory::duplicate() {
    auto memDupe = std::make_unique<darwinMappedMemory>();
    memDupe->fd = dup(fd);
    memDupe->memSize = memSize;
    memDupe->memBase = memBase;
    memDupe->isDupe = true;
    memDupe->ownsFd = true;
    return memDupe;
}

std::string darwinMappedMemory::generateUniqueShmName() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::string name = "/bora_shm_";
    for (int i = 0; i < 16; ++i) {
        char buf[2];
        snprintf(buf, sizeof(buf), "%x", dis(gen));
        name += buf;
    }
    return name;
}

void darwinMappedMemory::cleanup() {
    if (!isDupe) {
        if (memBase != nullptr && memBase != MAP_FAILED && memSize > 0) {
            munmap(memBase, memSize);
        }
    }

    if (fd != -1 && ownsFd) {
        close(fd);
    }
    fd = -1;
    memSize = 0;
    memBase = nullptr;
    offset = 0;
}
