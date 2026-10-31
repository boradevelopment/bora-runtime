// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: WasmSupervisorManager.h
 * Purpose: Manages how we as the user communicate and relay information to the supervisor.
 * Note: If supervisor is disabled, data returned and given will be stubs.
*/
#pragma once
#include "nNetworking/IDomainSocket.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <unordered_map>
#include <future>
#include <cstdint>
#include <memory>
#include "../../../common/networking/supervisor/ISupervisorInterface.h"
#include "nThread/ThreadPool.h"

class WasmSupervisorManager
{
public:
    WasmSupervisorManager();
    ~WasmSupervisorManager();

    void open();

    bool isConnected() const {return socket_->isConnected(); }
    [[nodiscard]] IDomainSocket* getSocket() const {return socket_.get();}

    // Sends a message asynchronously
    std::future<void> sendMessage(u32 commandType, const u8* data = nullptr, size_t size = 0);
    // Replies to a message
    std::future<void> reply(const SupervisorMessage& message,const u8* data = nullptr, size_t size = 0);
    // Sends message and waits for a response
    SupervisorMessage communicate(u32 commandType, const u8* data = nullptr, size_t size = 0);
    bool isRunning();
    using CommandCallback = std::function<void(const SupervisorMessage&, WasmSupervisorManager*)>;
    void hookCommand(u32 commandType, CommandCallback callback);
    void unhookCommand(u32 commandType);
    using ClientCallback = std::function<void(WasmSupervisorManager*)>;
    void onConnect(const ClientCallback& function);
    void onDisconnect(const ClientCallback& function);

private:
    struct OutgoingMessage {
        SupervisorMessage message;
        std::promise<void> completionPromise; // Notifies when sent
    };

    void supervisorLoop();
    void writeLoop();
    void readLoop();
    std::thread supervisorThread;
    std::thread supervisorThreadW;
    std::thread supervisorThreadR;
    sUniquePointer<IDomainSocket> socket_ = nullptr;
    std::atomic<bool> running_{false};

    // Thread safety & coordination
    std::mutex mutex_;
    std::condition_variable cvOutgoing_;
    std::queue<OutgoingMessage> outgoingQueue_;

    // Request-response tracking for synchronous communicate()
    u64 nextRequestId_ = 0;
    std::unordered_map<u64, std::promise<SupervisorMessage>> pendingRequests_;
    std::unordered_map<u32, CommandCallback> commandHooks_;
    sSharedPointer<Logger> logger;

    std::vector<ClientCallback> newClientHooks_;
    std::vector<ClientCallback> disconnectClientHooks_;
    std::string path;
    ThreadPool threadPool;
    std::mutex writeMutex;
};
