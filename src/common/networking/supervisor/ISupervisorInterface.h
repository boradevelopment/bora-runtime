// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: ISupervisorInterface.h
 * Purpose: Holds common interfaces for both runtime and supervisor
*/
#pragma once
#include <cstring>
#include <utility>
#include "../BufferRW.h"
#include "nNetworking/IDomainSocket.h"
#include "ISupervisorCommands.h"

class ISupervisorCodec
{
public:
    virtual ~ISupervisorCodec() = default;
    /**
     * Non-virtual wrapper for serializing the message object into a byte vector, including any necessary
     * length headers or formatting required for transmission.
     *
     * @param buffer Reference to the destination vector where bytes will be written.
     *               The vector is resized internally to fit the serialized data.
     * @return true on successful serialization, false otherwise.
     */
    bool serialize(vec8& buffer) const
    {
        return serializeFunc(buffer);
    };

    /**
     * Non-virtual wrapper for deserializing the message from a byte span.
     * This avoids virtual function default argument warnings while forwarding
     * the call to the protected implementation.
     *
     * @param buffer Read-only span containing the incoming packet bytes.
     * @param offset Byte offset indicating where to start reading within the buffer.
     *               Note: While an explicit offset allows for out-of-order or custom
     *               parsing, reading fields in order is strongly
     *               recommended for organization.
     * @return true if deserialization succeeded, false if the buffer was truncated or malformed.
     */
    bool deserialize(std::span<const u8> buffer, size_t offset = 0) // fix for default arguments in virtual warnings
    {
        return deserializeFunc(buffer, offset);
    }

protected:
    virtual bool deserializeFunc(std::span<const u8> buffer, size_t offset) = 0;
    virtual bool serializeFunc(vec8& buffer) const = 0;
};

struct SupervisorMessage : ISupervisorCodec {
    static u32 Magic()
    {
        return 0x42524F53; // "BROS"
    }

    u32 commandType = 0;
    u64 requestId = 0; // Used to match responses to requests
    vec8 payload;
protected:
    bool serializeFunc(vec8& buffer) const override
    {
        u32 payloadSize = static_cast<u32>(payload.size());

        VectorWriter writer(buffer);
        writer.write(commandType);
        writer.write(requestId);

        writer.write(payloadSize);
        if (payloadSize > 0) {
            writer.writeBytes(payload.data(), payloadSize);
        }

        return true;
    }

    bool deserializeFunc(const std::span<const u8> buffer, size_t offset) override
    {
        BufferReader reader(buffer);

        // 1. Read command type
        if (!reader.read(commandType))
        {
            return false;
        }

        // 2. Read request ID
        if (!reader.read(requestId))
        {
            return false;
        }

        if (commandType == 0 && requestId == 0)
        {
            return false;
        }

        // 3. Read payload size
        u32 payloadSize = 0;
        if (!reader.read(payloadSize)) return false;

        // 4. Read payload data
        payload.resize(payloadSize);
        if (payloadSize > 0) {
            if (!reader.readBytes(payload.data(), payloadSize)) {
                return false; // Buffer was truncated - invalid.
            }
        }



        return true;
    }
};

class SupervisorPacketReader {
public:
    SupervisorPacketReader(IDomainSocket* socket)
        : socket_(socket), headBuffer_(4) {}

    // Tries to read the next complete message.
    // Returns true if a message was successfully read, false on disconnect/error.
    bool pollAndReadMessage(SupervisorMessage& outMsg) {
        if (size == 0)
        {
            if (headBuffer_.size() != sizeof(u32)) headBuffer_.resize(sizeof(u32));
            auto recv = socket_->receive(headBuffer_.data(), sizeof(u32));
            if (recv != sizeof(u32) || headBuffer_.size() != sizeof(u32)) return false; // NOT AN U32 value
            BufferReader(headBuffer_.data(), headBuffer_.size()).read(size);
            if (size == 0)
            {
                headBuffer_.clear();
                return false;
            }
            headBuffer_.clear();
        }

        // If we don't have the data yet because of timeout, we should ensure we can still get it back.
        if (bodyBuffer_.size() != size) bodyBuffer_.resize(size);
        auto recv = socket_->receive(bodyBuffer_.data(), size);
        if (recv == 0) return false;

        if (recv != size)
        {
            bodyBuffer_.clear();
            return false;
        }

        bool success = outMsg.deserialize(bodyBuffer_);
        size = 0;
        bodyBuffer_.clear();

        return success;
    }

private:
    IDomainSocket* socket_;
    u32 size = 0;
    vec8 headBuffer_;   // Accumulates the rest of the packet body
    vec8 bodyBuffer_;   // Accumulates the rest of the packet body
};
