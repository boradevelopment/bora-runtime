// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: FileSystem.h
 * Purpose: The codec for BORA filesystem
*/
#pragma once
#include <utility>
#include "networking/supervisor/ISupervisorInterface.h"

namespace SupervisorCodecs
{
    class ReadFile : public ISupervisorCodec
    {
    public:
        i32 handle = -1;
        u64 offset = 0;
        u64 size = 0;
        u64 nmemb = 0;

        ReadFile() = default;
        ReadFile(i32 handle, u64 offset = 0, u64 size = 0, u64 nmemb = 0) : handle(handle), offset(offset), size(size), nmemb(nmemb) {}
        ReadFile(std::span<const u8> buffer, size_t offset = 0)
        {
            deserialize(buffer, offset);
        }

        i32* operator->() { return &handle; }
        i32& operator*() { return handle; }
        operator i32&() { return handle; }
        operator const i32&() const { return handle; }

    protected:
        bool serializeFunc(vec8& buffer) const override
        {
            buffer.resize(sizeof(handle) + sizeof(offset) + sizeof(size) + sizeof(nmemb));
            BufferWriter writer(buffer.data(), buffer.size());
            writer.write(handle);
            writer.write(offset);
            writer.write(size);
            writer.write(nmemb);

            return true;
        }

        // Deserializes bytes back into its own fields
        bool deserializeFunc(std::span<const u8> buffer, size_t offset) override
        {
            // Use a reader starting from the given offset
            BufferReader reader(buffer.subspan(offset));
            reader.read(handle);
            reader.read(this->offset);
            reader.read(size);
            reader.read(nmemb);

            return true;
        }
    };

    enum FileFlags : uint32_t {
        CAN_READ  = 1 << 0,
        CAN_WRITE = 1 << 1,
        CAN_CREATE = 1 << 2
    };

    class File : public ISupervisorCodec
    {
    public:
        u32 handle = -1; // given by supervisor
        u32 flags = 0; // Combination of FileFlags

        File() = default;
        File(u32 handle, u32 flags = 0) : handle(handle), flags(flags) {}
        File(std::span<const u8> buffer, size_t offset = 0)
        {
            deserialize(buffer, offset);
        }

        u32* operator->() { return &handle; }
        u32& operator*() { return handle; }
        operator u32&() { return handle; }
        operator const u32&() const { return handle; }
    protected:
        bool serializeFunc(vec8& buffer) const override
        {
            buffer.resize(8);
            BufferWriter writer(buffer.data(), buffer.size());
            writer.write(handle);
            writer.write(flags);

            return true;
        }

        // Deserializes bytes back into its own fields
        bool deserializeFunc(std::span<const u8> buffer, size_t offset) override
        {
            // Use a reader starting from the given offset
            BufferReader reader(buffer.subspan(offset));
            reader.read(handle);
            reader.read(flags);

            return true;
        }
    };
}