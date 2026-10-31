// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: SupervisorSocketManager.h
 * Purpose: ?
*/
#pragma once
#include <atomic>
#include <thread>
#include "../../common/networking/supervisor/ISupervisorInterface.h"
#include "nNetworking/IDomainSocket.h"
#include <future>
#include "nNetworking/DomainSocketFactory.h"
#include "nThread/ThreadPool.h"

class SupervisorSocketManager
{
    struct OutgoingMessage {
        SupervisorMessage message;
        std::promise<void> completionPromise; // Notifies when sent
    };

    struct ClientSession {
        u64 clientId;
        sUniquePointer<IDomainSocket> socket;
        vec8 receivedData;
        std::queue<OutgoingMessage> outgoingQueue;
        std::unordered_map<uint64_t, std::promise<SupervisorMessage>> pendingRequests_;
        std::mutex clientWriteMutex;
        std::mutex clientReadMutex;
        std::condition_variable cvOutgoing;
        std::thread readerThread;
        std::thread writerThread;

        std::atomic_bool disableReading = false;
        std::atomic_bool hasStoppedReading = false;

        ~ClientSession() {
            cvOutgoing.notify_all();
            if (readerThread.joinable()) readerThread.join();
            if (writerThread.joinable()) writerThread.join();
        }
    };

public:
    SupervisorSocketManager();

    void open();
    void close();

    IDomainSocketServer* getServer() const;
    bool isActive();

    // Sends a message asynchronously
    std::future<void> sendMessage(u64 clientID, u32 commandType, const u8* data = nullptr, size_t size = 0);
    // Replies to a message
    std::future<void> reply(const SupervisorMessage& message, u64 clientID, const u8* data = nullptr, size_t size = 0);
    // Sends message and waits for a response
    SupervisorMessage communicate(u64 clientID, u32 commandType, const u8* data = nullptr, size_t size = 0);
    IDomainSocket* getSocket(u64 clientID);
    ClientSession* getSession(i64 i64);
    using CommandCallback = std::function<void(u64 clientID, const SupervisorMessage&, SupervisorSocketManager*)>;
    void hookCommand(u32 commandType, CommandCallback callback);
    void unhookCommand(u32 commandType);
    using ClientCallback = std::function<void(u64 clientID, SupervisorSocketManager*)>;
    void onConnect(const ClientCallback& function);
    void onDisconnect(const ClientCallback& function);
private:
    void mainSupervisorLoop();
    void supervisorServerLoop(i64 clientId);
    void supervisorServerWriteLoop(ClientSession* client) const;
    void supervisorServerReadLoop(ClientSession* client);
    void supervisorNewClientHook(i64 clientId);
    uint64_t nextRequestId_ = 0;

    std::thread acceptThread_;
    // sUnorderedMap<u64, std::thread> clientThread;
    std::mutex clientsMutex_;
    std::atomic<bool> gRunning;
    std::unique_ptr<IDomainSocketServer> server;
    std::atomic<u64> nextClientId_{0};
    sSharedPointer<Logger> logger;
    std::string path;
    ThreadPool threadPool;
    std::unordered_map<u64, std::unique_ptr<ClientSession>> activeClients_;
    std::unordered_map<u32, CommandCallback> commandHooks_;
    std::vector<ClientCallback> newClientHooks_;
    std::vector<ClientCallback> disconnectClientHooks_;
};
