// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: Primitives.h
 * Purpose: Basic low level data payloads
*/
#pragma once
#include "networking/supervisor/ISupervisorInterface.h"

namespace SupervisorCodecs {

    class String : public ISupervisorCodec
    {
    public:
        sString payload;

        String() = default;
        String(sString payload) : payload(std::move(payload)) {}
        String(std::span<const u8> buffer, size_t offset = 0)
        {
            deserialize(buffer, offset);
        }

        sString* operator->() { return &payload; }
        const sString* operator->() const { return &payload; }

        sString& operator*() { return payload; }
        const std::string& operator*() const { return payload; }

        operator sString&() { return payload; }
        operator const sString&() const { return payload; }

    protected:
        // Serializes the class
        bool serializeFunc(vec8& buffer) const override
        {
            size_t payloadLen = payload.size();
            size_t totalSize = sizeof(payloadLen) + payloadLen;
            VectorWriter writer(buffer);
            if (payloadLen > 0) {
                writer.write(payloadLen);
                writer.writeBytes(payload.data(), payloadLen);
            }

            return true;
        }

        // Deserializes bytes back into its own fields
        bool deserializeFunc(std::span<const u8> buffer, size_t offset) override
        {
            // Use a reader starting from the given offset
            BufferReader reader(buffer.subspan(offset));
            size_t payloadLen = 0;
            if (!reader.read(payloadLen)) return false;
            if (reader.remaining() < payloadLen) {
                return false; // Truncated buffer
            }
            payload.resize(payloadLen);
            if (payloadLen > 0) {
                if (!reader.readBytes(payload.data(), payloadLen)) {
                    return false;
                }
            }

            return true;
        }
    };
}
