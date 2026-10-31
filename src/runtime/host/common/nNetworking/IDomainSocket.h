// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: IDomainSocket.h
 * Purpose: General class for OS Processes to use
*/
#pragma once
#include "ISocket.h"
#include <string>

class IDomainSocket : public ISocket {
public:
    ~IDomainSocket() override = default;

    // ISocket interface (pure virtual)
    bool send(const void* data, size_t size) override = 0;
    ssize_t receive(void* buffer, size_t max_size) override = 0;
    bool isConnected() const override = 0;
    void close() override = 0;
    bool reconnect() override = 0;

    // Advanced IPC: File descriptor passing (POSIX SCM_RIGHTS, etc.)
    // Note: Platforms like Windows Named Pipes may return false/not supported.
    virtual bool send_fd(int fd, const void* data = nullptr, size_t size = 0) = 0;
    virtual int receive_fd(void* data_out = nullptr, size_t max_size = 0, size_t* received_size = nullptr) = 0;

    // Getter for path if bound
    virtual const std::string& get_path() const = 0;
};

class IDomainSocketServer {
public:
    virtual ~IDomainSocketServer() = default;

    virtual bool isListening() const = 0;

    virtual std::unique_ptr<IDomainSocket> accept() = 0;

    virtual void close() = 0;
};
