// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: PosixDomainSocket.h
 * Purpose: ?
*/
#pragma once
#include "nNetworking/IDomainSocket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <utility>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

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

    explicit PosixDomainSocket(std::string  path) : socket_fd_(-1), is_owner_(true), socket_path_(std::move(path)) {
        PosixDomainSocket::reconnect();
    }

    ~PosixDomainSocket() override {
        PosixDomainSocket::close();
    }

    // should block.
    bool send(const void* data, size_t size) override {
        if (socket_fd_ == -1 || data == nullptr || size == 0) {
            return false;
        }

        while (true) {
            ssize_t n = ::send(socket_fd_, data, size, 0);

            if (n >= 0) {
                return true;
            }

            int err = errno;
            if (err == EINTR) {
                continue; // Interrupted by a signal, safe to retry the whole send
            }

            if (err == EPIPE || err == ECONNRESET) {
                close();
            }

            return false;
        }
    }

    ssize_t receive(void* buffer, size_t size) override {
        if (socket_fd_ == -1 || buffer == nullptr || size == 0) {
            return -1;
        }

        pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;

        while (true) {
            int ret = ::poll(&pfd, 1, 100);

            if (socket_fd_ == -1) return 0;

            if (ret < 0) {
                if (errno == EINTR) continue;
                return -1; // Error or interrupted
            }

            if (ret == 0) {
                return 0;
            }

            if (pfd.revents & POLLIN) {
                ssize_t n = ::recv(socket_fd_, buffer, size, 0);
                if (n < 0) {
                    if (errno == EINTR) continue;
                    return -1;
                }
                if (n == 0) {
                    close();
                    return 0;
                }
                return n;
            }

            if (pfd.revents & (POLLHUP | POLLERR)) {
                return -1;
            }
        }
    }

    bool send_fd(int fd, const void* data, size_t size) override {
        if (socket_fd_ == -1 || fd < 0) {
            return false;
        }

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov[1];
        char dummy = 0;
        if (data == nullptr || size == 0) {
            iov[0].iov_base = &dummy;
            iov[0].iov_len = 1;
        } else {
            iov[0].iov_base = const_cast<void*>(data);
            iov[0].iov_len = size;
        }
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        char cmsg_buf[CMSG_SPACE(sizeof(int))];
        std::memset(cmsg_buf, 0, sizeof(cmsg_buf));

        msg.msg_control = cmsg_buf;
        msg.msg_controllen = sizeof(cmsg_buf);

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));

        std::memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

        while (true) {
            ssize_t n = ::sendmsg(socket_fd_, &msg, 0);
            if (n != -1) {
                return true;
            }

            int err = errno;
            if (err == EINTR) continue;

            std::cerr << "sendmsg failed: " << std::strerror(err)
                      << " (errno: " << err << ")" << std::endl;
            return false;
        }
    }

    int receive_fd(void* data_out, size_t max_size, size_t* received_size) override {
        if (socket_fd_ == -1) {
            return -1;
        }

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov[1];
        char dummy;
        if (data_out == nullptr || max_size == 0) {
            iov[0].iov_base = &dummy;
            iov[0].iov_len = 1;
        } else {
            iov[0].iov_base = data_out;
            iov[0].iov_len = max_size;
        }
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        char cmsg_buf[CMSG_SPACE(sizeof(int))];
        std::memset(cmsg_buf, 0, sizeof(cmsg_buf));
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = sizeof(cmsg_buf);
        ssize_t n = 0;

        while (true) {
            n = ::recvmsg(socket_fd_, &msg, 0);
            if (n >= 0) {
                break;
            }

            int err = errno;
            if (err == EINTR) continue;

            std::cerr << "recvmsg failed: " << std::strerror(err) << " (errno: " << err << ")" << std::endl;
            return -1;
        }

        if (received_size != nullptr) {
            if (data_out == nullptr || max_size == 0) {
                *received_size = 0;
            } else {
                *received_size = static_cast<size_t>(n);
            }
        }

        int received_fd = -1;
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg != nullptr && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            std::memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
        }

        return received_fd;
    }

    bool isConnected() const override {
        return socket_fd_ != -1;
    }

    void close() override {
        if (is_owner_ && socket_fd_ != -1) {
            if (epoll_fd_ != -1) ::close(epoll_fd_);
            shutdown(socket_fd_, SHUT_RDWR);
            ::close(socket_fd_);
        }
        socket_fd_ = -1;
        epoll_fd_ = -1;
    }

    bool reconnect() override {
        signal(SIGPIPE, SIG_IGN);
        int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        if (sock == -1) {
            return false;
        }

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        if (socket_path_.length() >= sizeof(addr.sun_path)) {
            ::close(sock);
            return false;
        }
        std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
            ::close(sock);
            return false;
        }

        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &DOMAIN_SOCKET_BUFFER_SIZE, sizeof(DOMAIN_SOCKET_BUFFER_SIZE));
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &DOMAIN_SOCKET_BUFFER_SIZE, sizeof(DOMAIN_SOCKET_BUFFER_SIZE));
        ::fcntl(socket_fd_, F_SETFL, O_NONBLOCK);
        socket_fd_ = sock;
        return true;
    }

    const std::string& get_path() const override {
        return socket_path_;
    }

    static bool create_pair(std::unique_ptr<IDomainSocket> sockets[2]) {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) == -1) return false;
        sockets[0] = std::make_unique<PosixDomainSocket>(sv[0]);
        sockets[1] = std::make_unique<PosixDomainSocket>(sv[1]);
        return true;
    }
};

class PosixDomainSocketServer : public IDomainSocketServer {
private:
    int server_fd_;
    std::string socket_path_;

public:
    explicit PosixDomainSocketServer(const std::string& path)
        : server_fd_(-1), socket_path_(path) {

        server_fd_ = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        if (server_fd_ == -1) return;
        unlink(path.c_str());

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        if (path.length() >= sizeof(addr.sun_path)) {
            ::close(server_fd_);
            server_fd_ = -1;
            return;
        }
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
            ::close(server_fd_);
            server_fd_ = -1;
            return;
        }

        chmod(path.c_str(), 0666);

        if (listen(server_fd_, SOMAXCONN) == -1) {
            ::close(server_fd_);
            server_fd_ = -1;
        }
    }

    ~PosixDomainSocketServer() override {
        PosixDomainSocketServer::close();
    }

    bool isListening() const override {
        return server_fd_ != -1;
    }

    std::unique_ptr<IDomainSocket> accept() override {
        if (server_fd_ == -1) return nullptr;

        struct sockaddr_un client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = ::accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (client_fd == -1) {
            return nullptr;
        }

        setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &DOMAIN_SOCKET_BUFFER_SIZE, sizeof(DOMAIN_SOCKET_BUFFER_SIZE));
        setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &DOMAIN_SOCKET_BUFFER_SIZE, sizeof(DOMAIN_SOCKET_BUFFER_SIZE));
        ::fcntl(server_fd_, F_SETFL, O_NONBLOCK);

        return std::make_unique<PosixDomainSocket>(client_fd, true);
    }

    void close() override {
        if (server_fd_ != -1) {
            shutdown(server_fd_, SHUT_RDWR);
            ::close(server_fd_);
            server_fd_ = -1;
            unlink(socket_path_.c_str());
        }
    }
};