// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "SupervisorSocketManager.h"

#include <ranges>

#include "nThread/ThreadUtilities.h"
#include "supervisor/SupervisorGlobal.h"


SupervisorSocketManager::SupervisorSocketManager() : gRunning(false), threadPool(std::thread::hardware_concurrency() == 0 ? 4 : (std::thread::hardware_concurrency() * 2))
{
    logger = LogManager::instance().getLogger("bora.supervisor");
}

void SupervisorSocketManager::open()
{
    gRunning = true;
    // todo: linux only
    std::filesystem::create_directories("/run/bora");
    path = std::format("/run/bora/bora_supervisor.{}.sock", BORA_VERSION);
    server = DomainSocketFactory::createServer(path);
    acceptThread_ = std::thread(&SupervisorSocketManager::mainSupervisorLoop, this);
    bora::thread::nameThread(acceptThread_, "svAccept");
}

void SupervisorSocketManager::close()
{
    gRunning = false;
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& client : activeClients_ | std::views::values)
    {
        client->socket->close();
        // if (clientThread[client->clientId].joinable()) clientThread[client->clientId].join();
    }
    // clientThread.clear();
    if (acceptThread_.joinable()) acceptThread_.join();
    nextClientId_ = 0;
    server->close();
}

IDomainSocketServer* SupervisorSocketManager::getServer() const
{
    return server.get();
}

bool SupervisorSocketManager::isActive()
{
    return gRunning;
}

std::future<void> SupervisorSocketManager::sendMessage(u64 clientID, u32 commandType, const u8* data, size_t size)
{
    SupervisorMessage msg;
    msg.commandType = commandType;
    msg.requestId = 0;
    if (data)
    {
        msg.payload.assign(data, data + size);
    }
    vec8 serializedBuffer;
    bool result = msg.serialize(serializedBuffer);

    OutgoingMessage outMsg;
    outMsg.message = std::move(msg);
    auto future = outMsg.completionPromise.get_future();
    if (!result)
    {
        outMsg.completionPromise.set_value();
        return future;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = activeClients_.find(clientID);
        if (it != activeClients_.end()) {
            {
                std::lock_guard<std::mutex> clientLock(it->second->clientWriteMutex);
                it->second->outgoingQueue.push(std::move(outMsg));
            }
            it->second->cvOutgoing.notify_one();
        } else {
            // Client not found, satisfy promise immediately to prevent hanging
            outMsg.completionPromise.set_value();
        }
    }
    return future;
}

std::future<void> SupervisorSocketManager::reply(const SupervisorMessage& message, u64 clientID, const u8* data,
    size_t size)
{
    SupervisorMessage msg;
    msg.requestId = message.requestId;
    if (data)
    {
        msg.payload.assign(data, data + size);
    }
    vec8 serializedBuffer;
    bool result = msg.serialize(serializedBuffer);
    OutgoingMessage outMsg;
    outMsg.message = std::move(msg);
    auto future = outMsg.completionPromise.get_future();
    if (!result)
    {
        outMsg.completionPromise.set_value();
        return future;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = activeClients_.find(clientID);
        if (it != activeClients_.end()) {
            {
                std::lock_guard<std::mutex> clientLock(it->second->clientWriteMutex);
                it->second->outgoingQueue.push(std::move(outMsg));
            }
            it->second->cvOutgoing.notify_one();
        } else {
            // Client not found, satisfy promise immediately to prevent hanging
            outMsg.completionPromise.set_value();
        }
    }

    return future;
}

SupervisorMessage SupervisorSocketManager::communicate(u64 clientID, u32 commandType, const u8* data, size_t size)
{
    uint64_t currentReqId = 0;
    std::future<SupervisorMessage> futureResponse;
    IDomainSocket* targetSocket = nullptr;

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = activeClients_.find(clientID);
        if (it == activeClients_.end() || !it->second->socket->isConnected()) {
            throw std::runtime_error("Client not connected or not found");
        }

        targetSocket = it->second->socket.get();
        currentReqId = ++nextRequestId_;

        {
            std::lock_guard<std::mutex> lock2(it->second->clientReadMutex);
            auto& promise = it->second->pendingRequests_[currentReqId];
            futureResponse = promise.get_future();
        }
    }


    // Prepare message with the unique request ID
    SupervisorMessage msg;
    msg.commandType = commandType;
    msg.requestId = currentReqId;
    if (data)
    {
        msg.payload.assign(data, data + size);
    }
    vec8 serializedBuffer;
    bool result = msg.serialize(serializedBuffer);
    if (!result) return {};

    OutgoingMessage outMsg;
    outMsg.message = std::move(msg);

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = activeClients_.find(clientID);
        if (it != activeClients_.end()) {
            // Lock the individual client's write queue
            std::lock_guard<std::mutex> clientLock(it->second->clientWriteMutex);
            it->second->outgoingQueue.push(std::move(outMsg));
            it->second->cvOutgoing.notify_one();
        }
    }

    // Block caller until the server thread receives the matching response
    return futureResponse.get();
}

IDomainSocket* SupervisorSocketManager::getSocket(u64 clientID)
{
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = activeClients_.find(clientID);
        if (it == activeClients_.end() || !it->second->socket->isConnected()) {
            throw std::runtime_error("Client not connected or not found");
        }

        return it->second->socket.get();
    }
}

SupervisorSocketManager::ClientSession* SupervisorSocketManager::getSession(i64 clientId)
{
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = activeClients_.find(clientId);
        if (it == activeClients_.end() || !it->second->socket->isConnected()) {
            throw std::runtime_error("Client not connected or not found");
        }

        return it->second.get();
    }
}

void SupervisorSocketManager::hookCommand(u32 commandType, CommandCallback callback)
{
    if (gRunning)
    {
        LOG_ERROR(logger) << "Attempted to hook a command while open! Illegal!";
        return;
    }
    std::lock_guard<std::mutex> lock(clientsMutex_);
    commandHooks_[commandType] = std::move(callback);
}

void SupervisorSocketManager::unhookCommand(u32 commandType)
{
    if (gRunning)
    {
        LOG_ERROR(logger) << "Attempted to hook a command while open! Illegal!";
        return;
    }
    std::lock_guard<std::mutex> lock(clientsMutex_);
    commandHooks_.erase(commandType);
}

void SupervisorSocketManager::onConnect(const ClientCallback& function)
{
    if (gRunning)
    {
        LOG_ERROR(logger) << "Attempted to hook a command while open! Illegal!";
        return;
    }
    newClientHooks_.push_back(function);
}

void SupervisorSocketManager::onDisconnect(const ClientCallback& function)
{
    if (gRunning)
    {
        LOG_ERROR(logger) << "Attempted to hook a command while open! Illegal!";
        return;
    }
    disconnectClientHooks_.push_back(function);
}

void SupervisorSocketManager::mainSupervisorLoop()
{
   if (!server || !server->isListening()) {
       LOG_ERROR(logger) << "Failed to start domain socket server\n";
       return;
   }

   LOG_INFO_VERBOSE(logger, 1) << "Started Supervisor Server @ " << path;

   while (gRunning) {
   auto clientSocket = server->accept();
   if (!clientSocket) {
       if (!gRunning) break;
       continue;
   }

    u64 clientId = nextClientId_++;

    {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    activeClients_[clientId] = std::make_unique<ClientSession>();
    activeClients_[clientId]->clientId = clientId;
    activeClients_[clientId]->socket = std::move(clientSocket);
    }

    std::thread(&SupervisorSocketManager::supervisorServerLoop, this, clientId).detach();
    std::thread(&SupervisorSocketManager::supervisorNewClientHook, this, clientId).detach();
    }
}

void SupervisorSocketManager::supervisorServerLoop(i64 clientId)
{
      bora::thread::nameThread(std::format("svServer{}", clientId).c_str());
        ClientSession* client = nullptr;
        IDomainSocket* clientSocket = nullptr;
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            auto it = activeClients_.find(clientId);
            if (it != activeClients_.end()) {
                clientSocket = it->second->socket.get();
                client = it->second.get();
            }
        }

        if (!clientSocket) {
            for (const auto & disconnect_client_hook : disconnectClientHooks_)
            {
                disconnect_client_hook(clientId, this);
            }
            return; // Client session not found
        }

    client->writerThread = std::thread(&SupervisorSocketManager::supervisorServerWriteLoop, this, client);
    client->readerThread = std::thread(&SupervisorSocketManager::supervisorServerReadLoop, this, client);

    while (gRunning && client->socket->isConnected())
    {
        std::this_thread::yield();
    }
    if (client->socket->isConnected())      // we are supposed to shutdown. close the socket safely
    {
     client->socket->close();
    }

    if (client->writerThread.joinable()) client->writerThread.join();
    if (client->readerThread.joinable()) client->readerThread.join();

    for (const auto & disconnect_client_hook : disconnectClientHooks_)
    {
        disconnect_client_hook(clientId, this);
    }
    std::lock_guard<std::mutex> lock(clientsMutex_);
    activeClients_.erase(clientId);
}

void SupervisorSocketManager::supervisorServerWriteLoop(ClientSession* client) const
{
    bora::thread::nameThread(std::format("svServer{}W", client->clientId).c_str());
    vec8 serializedBuffer;
    while (client->socket->isConnected() && gRunning)
    {
        {
            std::unique_lock<std::mutex> lock(client->clientWriteMutex);
            client->cvOutgoing.wait_for(lock, std::chrono::milliseconds(5), [&] {
                return !client->outgoingQueue.empty() || !gRunning || !client->socket->isConnected();
            });

            if (!gRunning || !client->socket->isConnected()) return;
        }


        // 2. Pop and process messages safely
        while (true) {
            std::unique_lock<std::mutex> lock(client->clientWriteMutex);
            if (client->outgoingQueue.empty()) {
                break;
            }

            auto msg = std::move(client->outgoingQueue.front());
            client->outgoingQueue.pop();
            lock.unlock();

            serializedBuffer.clear();
            if (msg.message.serialize(serializedBuffer)) {
                u32 bufferSize = serializedBuffer.size();
                bool sentOk = client->socket->send(&bufferSize, sizeof(u32));
                if (!sentOk) {
                    LOG_WARN(logger) << "Failed to send message to server";
                    continue;
                }
                sentOk = client->socket->send(serializedBuffer.data(), serializedBuffer.size());
                if (!sentOk) {
                    LOG_WARN(logger) << "Failed to send message to server";
                }
            }

            msg.completionPromise.set_value();
        }
    }
}

void SupervisorSocketManager::supervisorServerReadLoop(ClientSession* client)
{
    u64 clientId = client->clientId;
    bora::thread::nameThread(std::format("svServer{}R", clientId).c_str());
    SupervisorPacketReader packetReader(client->socket.get());
    while (gRunning && client->socket->isConnected())
    {
        if (client->disableReading)
        {
            client->hasStoppedReading = true;
            continue;
        }
        client->hasStoppedReading = false;

        SupervisorMessage incomingMsg;
        if (packetReader.pollAndReadMessage(incomingMsg))
        {
            if (incomingMsg.requestId != 0) {
                std::lock_guard<std::mutex> lock(client->clientReadMutex);
                auto it = client->pendingRequests_.find(incomingMsg.requestId);
                if (it != client->pendingRequests_.end()) {
                    it->second.set_value(std::move(incomingMsg));
                    client->pendingRequests_.erase(it);
                    continue;
                }
            }

            // Handle unsolicited incoming command from supervisor
            CommandCallback callback = nullptr;
            {
                auto it = commandHooks_.find(incomingMsg.commandType);
                if (it != commandHooks_.end()) {
                    callback = it->second;
                }
            }

            if (callback) {
                threadPool.enqueue([callback, clientId, msg = std::move(incomingMsg), this]() {
                callback(clientId, msg, this);
                });
            } else {
                LOG_WARN_VERBOSE(logger, 1) << "Command " << incomingMsg.commandType << " not implemented";
            }
        }
    }

    for (auto & request : client->pendingRequests_) // set unfinished requests as empty.
    {
        request.second.set_value(SupervisorMessage());
    }
}

void SupervisorSocketManager::supervisorNewClientHook(i64 clientId)
{
    bora::thread::nameThread("svNewCliHook");
    for (const auto & new_client_hook : newClientHooks_)
    {
        new_client_hook(clientId, this);
    }
}
