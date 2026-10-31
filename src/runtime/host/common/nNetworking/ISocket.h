// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: ISocket.h
 * Purpose: Just a abstract Socket class
*/
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

class ISocket {
public:
    virtual ~ISocket() = default;

    virtual bool send(const void* data, size_t size) = 0;
    virtual ssize_t receive(void* buffer, size_t max_size) = 0;

    virtual bool isConnected() const = 0;
    virtual void close() = 0;
    virtual bool reconnect() = 0;
};