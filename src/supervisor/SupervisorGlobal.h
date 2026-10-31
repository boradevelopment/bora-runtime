// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: SupervisorGlobal.h
 * Purpose: Global instance that holds data that any supervisor class can access
*/
#pragma once
#include "client/SupervisorClient.h"
#include "networking/SupervisorSocketManager.h"

class SupervisorGlobal
{
public:
    static SupervisorGlobal& instance()
    {
        static SupervisorGlobal globalInstance;
        return globalInstance;
    }

    SupervisorGlobal(const SupervisorGlobal&) = delete;
    SupervisorGlobal& operator=(const SupervisorGlobal&) = delete;
    SupervisorGlobal(SupervisorGlobal&&) = delete;
    SupervisorGlobal& operator=(SupervisorGlobal&&) = delete;

    SupervisorSocketManager* getSocketManager();

    void addClient(i64 clientId);
    void removeClient(i64 clientId);
    std::shared_ptr<SupervisorClient> getClient(i64 clientId);

    std::atomic<bool> keep_running = true;
private:
    SupervisorGlobal() = default;
    ~SupervisorGlobal() = default;

    std::mutex mutex_;
    std::unordered_map<i64, std::shared_ptr<SupervisorClient>> clients_;
    SupervisorSocketManager manager;
};
