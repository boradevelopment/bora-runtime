// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: PosixDomainSocket.h
 * Purpose: ?
*/
#pragma once
#include "nNetworking/IDomainSocket.h"
#include <sys/socket.h>

static constexpr int DOMAIN_SOCKET_BUFFER_SIZE = 256 * 1024;

class PosixDomainSocket : public IDomainSocket {
private:
    int epoll_fd_ = -1;
    int socket_fd_;
    bool is_owner_{};
    std::string socket_path_;

public:
    explicit PosixDomainSocket(int fd, bool owner = true)
        : socket_fd_(fd), is_owner_(owner) {}

    explicit PosixDomainSocket(std::string  path);

    ~PosixDomainSocket() override;

    // should block.
    bool send(const void* data, size_t size) override;

    ssize_t receive(void* buffer, size_t size) override;

    bool send_fd(int fd, const void* data, size_t size) override;

    int receive_fd(void* data_out, size_t max_size, size_t* received_size) override;

    bool isConnected() const override;

    void close() override;

    bool reconnect() override;

    const std::string& get_path() const override;

    static bool create_pair(std::unique_ptr<IDomainSocket> sockets[2]);
};

class PosixDomainSocketServer : public IDomainSocketServer {
private:
    int server_fd_;
    std::string socket_path_;

public:
    explicit PosixDomainSocketServer(const std::string& path);

    ~PosixDomainSocketServer() override;

    bool isListening() const override;

    std::unique_ptr<IDomainSocket> accept() override;

    void close() override;
};