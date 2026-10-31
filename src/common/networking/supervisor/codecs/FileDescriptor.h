// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: FileDescriptor.h
 * Purpose: Supervisor codec for handling FileDescriptors
*/
#pragma once
#include <utility>
#include "Primitives.h"
#include "networking/supervisor/ISupervisorInterface.h"

namespace SupervisorCodecs
{
    enum SupervisorFileDescriptorType : u32 {
        WasmMemoryMapping = 0x01,
        TAZAArchiveMapping = 0x02
    };

    class GenericFileDescriptor : public ISupervisorCodec
    {
    public:
        virtual u32 GetType() = 0;
    };

    // Thi
    class WasmMemoryMappingFileDescriptor : public GenericFileDescriptor {
    public:
         u32 GetType() override
         {
             return WasmMemoryMapping;
         }
        WasmMemoryMappingFileDescriptor() = default;
        WasmMemoryMappingFileDescriptor(std::span<const u8> buffer, size_t offset = 0)
        {
            deserialize(buffer, offset);
        }


    protected:
        // Nothing to serialize as the recieve process is managed by the socket.
        // Only metadata will be serialized in GenericFileDescriptors to know whats which.
        // WasmMemoryMappings are very general.
        bool serializeFunc(vec8& buffer) const override
        {
            return true;
        }

        // Deserializes bytes back into its own fields
        bool deserializeFunc(std::span<const u8> buffer, size_t offset) override
        {
            return true;
        }
    };

    class TAZAArchiveMappingFileDescriptor : public GenericFileDescriptor {
    public:
        sString appId;
        TAZAArchiveMappingFileDescriptor() = default;
        TAZAArchiveMappingFileDescriptor(sString appId) : appId(std::move(appId)) {}
        TAZAArchiveMappingFileDescriptor(std::span<const u8> buffer, size_t offset = 0)
        {
            deserialize(buffer, offset);
        }

        u32 GetType() override
        {
            return TAZAArchiveMapping;
        }

    protected:
        // In this code, we will have a special string for what ID this archive is.
        bool serializeFunc(vec8& buffer) const override
        {;
            return String(appId).serialize(buffer);
        }

        // Deserializes bytes back into its own fields
        bool deserializeFunc(std::span<const u8> buffer, size_t offset) override
        {
            auto appIdString = String(buffer, offset);
            if (appIdString.payload.empty()) return false;
            appId = appIdString;
            return true;
        }
    };

    class FileDescriptorFactory {
    public:
        template <SupervisorFileDescriptorType CmdType>
        static constexpr auto GetClassType() {
            if constexpr (CmdType == WasmMemoryMapping) {
                return std::type_identity<WasmMemoryMappingFileDescriptor>{};
            }
            else if constexpr (CmdType == TAZAArchiveMapping) {
                return std::type_identity<TAZAArchiveMappingFileDescriptor>{};
            }
            else {
                static_assert(sizeof(CmdType) == 0, "Unsupported FileDescriptorType!");
            }
        }
    };


    template <typename T>
    requires std::derived_from<T, GenericFileDescriptor>
    class GeneralFileDescriptor : public ISupervisorCodec
    {
    public:
        u32 type = 0;
        sUniquePointer<T> fileDescriptor;

        GeneralFileDescriptor() = default;
        GeneralFileDescriptor(sUniquePointer<T> fileDescriptor) : fileDescriptor(std::move(fileDescriptor))
        {
            if (this->fileDescriptor)
            {
                type = this->fileDescriptor->GetType();
            }
        }
        GeneralFileDescriptor(std::span<const u8> buffer, size_t offset = 0)
        {
            deserialize(buffer, offset);
        }

        ISupervisorCodec* operator->() { return fileDescriptor.get(); }
    protected:
        bool serializeFunc(vec8& buffer) const override
        {
            VectorWriter writer(buffer);
            writer.write(type);
            fileDescriptor->serialize(buffer);

            return true;
        }

        // Deserializes bytes back into its own fields
        bool deserializeFunc(std::span<const u8> buffer, size_t offset) override
        {
            // Use a reader starting from the given offset
            BufferReader reader(buffer.subspan(offset));
            reader.read(type);
            fileDescriptor = std::make_unique<T>(buffer, offset+sizeof(type));

            return true;
        }
    };
}
