// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "WasmSupervisorManager.h"
#include "nNetworking/DomainSocketFactory.h"
#include "nThread/ThreadUtilities.h"

WasmSupervisorManager::WasmSupervisorManager() : threadPool(4)
{
    running_ = false;
}

WasmSupervisorManager::~WasmSupervisorManager()
{
    running_ = false;
    socket_->close(); // we need to shutdown it manually so threads can stop loops
    supervisorThread.join();
    cvOutgoing_.notify_all();
}

void WasmSupervisorManager::open()
{
    running_ = true;
    logger = LogManager::instance().getLogger("bora.supervisor");
    // todo: linux only
    path = std::format("/run/bora/bora_supervisor.{}.sock", BORA_VERSION);
    socket_ = DomainSocketFactory::createClient(path);
    supervisorThread = std::thread(&WasmSupervisorManager::supervisorLoop, this);
    bora::thread::nameThread(supervisorThread, "svClient");
}

std::future<void>  WasmSupervisorManager::sendMessage(u32 commandType, const u8* data, size_t size)
{
    SupervisorMessage msg;
    msg.commandType = commandType;
    msg.requestId = 0;
    if (data)
    {
        msg.payload.assign(data, data + size);
    }

    OutgoingMessage outMsg;
    outMsg.message = std::move(msg);
    auto future = outMsg.completionPromise.get_future();
    {
        std::lock_guard<std::mutex> lock(writeMutex);
        outgoingQueue_.push(std::move(outMsg));
    }
    cvOutgoing_.notify_one();

    return future;
}

std::future<void> WasmSupervisorManager::reply(const SupervisorMessage& message, const u8* data, size_t size)
{
    SupervisorMessage msg;
    msg.requestId = message.requestId; // 0 indicates a fire-and-forget message
    if (data)
    {
        msg.payload.assign(data, data + size);
    }

    OutgoingMessage outMsg;
    outMsg.message = std::move(msg);
    auto future = outMsg.completionPromise.get_future();
    {
        std::lock_guard<std::mutex> lock(writeMutex);
        outgoingQueue_.push(std::move(outMsg));
    }
    cvOutgoing_.notify_one();

    return future;
}

SupervisorMessage WasmSupervisorManager::communicate(u32 commandType, const u8* data, size_t size)
{
    std::future<SupervisorMessage> futureResponse;

    uint64_t currentReqId = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentReqId = ++nextRequestId_;

        // Set up a promise that will be fulfilled when the response arrives
        auto& promise = pendingRequests_[currentReqId];
        futureResponse = promise.get_future();
    }

    SupervisorMessage msg;
    msg.commandType = commandType;
    msg.requestId = currentReqId;
    if (data)
    {
        msg.payload.assign(data, data + size);
    }

    OutgoingMessage outMsg;
    outMsg.message = std::move(msg);
    auto future = outMsg.completionPromise.get_future();
    {
        std::lock_guard<std::mutex> lock(writeMutex);
        outgoingQueue_.push(std::move(outMsg));
    }
    cvOutgoing_.notify_one();
    future.wait();

    // Block caller until the supervisor thread receives the response
    return futureResponse.get();
}

bool WasmSupervisorManager::isRunning()
{
    return running_;
}

void WasmSupervisorManager::hookCommand(u32 commandType, CommandCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    commandHooks_[commandType] = std::move(callback);
}

void WasmSupervisorManager::unhookCommand(u32 commandType)
{
    std::lock_guard<std::mutex> lock(mutex_);
    commandHooks_.erase(commandType);
}

void WasmSupervisorManager::onConnect(const ClientCallback& function)
{
    newClientHooks_.push_back(function);
}

void WasmSupervisorManager::onDisconnect(const ClientCallback& function)
{
    disconnectClientHooks_.push_back(function);
}

void WasmSupervisorManager::supervisorLoop()
{
    bora::thread::nameThread("svLoop");
    auto time = std::chrono::steady_clock::now();
    while (!socket_->isConnected() && running_)
    {
        auto newTime = std::chrono::steady_clock::now();
        using namespace std::chrono_literals;
        if (newTime - time >= 1s)
        {
            for (const auto & disconnect_client_hook : disconnectClientHooks_)
            {
                disconnect_client_hook(this);
            }
            return;
        }
        socket_->reconnect();
    }

    std::thread([this]()
    {
        bora::thread::nameThread("svNewCliHook");
        for (const auto & new_client_hook : newClientHooks_)
        {
            new_client_hook(this);
        }
    }).detach();

    supervisorThreadW = std::thread(&WasmSupervisorManager::writeLoop, this);
    supervisorThreadR = std::thread(&WasmSupervisorManager::readLoop, this);
    while (running_ && socket_->isConnected())
    {
       std::this_thread::yield();
    }
    if (socket_->isConnected()) // we are supposed to shutdown. close the socket safely
    {
        socket_->close();
    }
    supervisorThreadR.join();
    supervisorThreadW.join();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [reqId, promise] : pendingRequests_) {
            // Set an empty or error message to unblock futures
            promise.set_value(SupervisorMessage{});
        }
        pendingRequests_.clear();
    }

    for (const auto & disconnect_client_hook : disconnectClientHooks_)
    {
        disconnect_client_hook(this);
    }
}

void WasmSupervisorManager::writeLoop()
{
    vec8 serializedBuffer;
    bora::thread::nameThread("svWrite");
    while (running_ && socket_->isConnected()){
        {
            std::unique_lock<std::mutex> lock(writeMutex);
            // Wait briefly for messages or exit condition
            cvOutgoing_.wait_for(lock, std::chrono::milliseconds(5), [this] {
                return !outgoingQueue_.empty() || !running_;
            });

            if (!running_) break;
        }

        while (true) {
            std::unique_lock<std::mutex> lock(writeMutex);
            if (outgoingQueue_.empty()) {
                break;
            }

            auto msg = std::move(outgoingQueue_.front());
            outgoingQueue_.pop();
            lock.unlock();

            serializedBuffer.clear();
            if (msg.message.serialize(serializedBuffer)) {
                u32 bufferSize = serializedBuffer.size();
                bool sentOk = socket_->send(&bufferSize, sizeof(u32));
                if (!sentOk) {
                    LOG_WARN(logger) << "Failed to send message to server";
                }
                sentOk = socket_->send(serializedBuffer.data(), serializedBuffer.size());
                if (!sentOk) {
                    LOG_WARN(logger) << "Failed to send message to server";
                }
            }

            msg.completionPromise.set_value();
        }
    }
}

void WasmSupervisorManager::readLoop()
{
    bora::thread::nameThread("svRead");
    SupervisorPacketReader packetReader(socket_.get());

    // 3. Main event-driven loop
    while (running_ && socket_->isConnected())
    {
        SupervisorMessage incomingMsg;
        if(packetReader.pollAndReadMessage(incomingMsg))
        {
            if (incomingMsg.requestId != 0)
            {
                std::lock_guard<std::mutex> lock(mutex_); // Ensure thread safety!
                auto it = pendingRequests_.find(incomingMsg.requestId);
                if (it != pendingRequests_.end())
                {
                    it->second.set_value(std::move(incomingMsg));
                    pendingRequests_.erase(it);
                    continue;
                }
            }

            CommandCallback callback = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_); // Ensure thread safety!
                auto it = commandHooks_.find(incomingMsg.commandType);
                if (it != commandHooks_.end()) {
                    callback = it->second;
                }
            }

            if (callback) {
                // Offload heavy processing to your thread pool so readLoop stays free
                threadPool.enqueue([callback, msg = std::move(incomingMsg), this]() {
                    callback(msg, this);
                });
            } else {
                LOG_WARN(logger) << "Command " << incomingMsg.commandType << " not implemented";
            }
        }
    }
}
