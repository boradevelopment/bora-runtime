// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "SupervisorGlobal.h"

SupervisorSocketManager* SupervisorGlobal::getSocketManager()
{
    return &manager;
}

void SupervisorGlobal::addClient(i64 clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    clients_[clientId] = std::make_shared<SupervisorClient>(clientId);
}

void SupervisorGlobal::removeClient(i64 clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(clientId);
}

std::shared_ptr<SupervisorClient> SupervisorGlobal::getClient(i64 clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(clientId);
    if (it != clients_.end()) {
        return it->second;
    }
    return nullptr;
}
