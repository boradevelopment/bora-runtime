// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: HookTemplate.h
 * Purpose: ?
*/
#pragma once
#include "supervisor/SupervisorGlobal.h"
#include "supervisor/networking/SupervisorSocketManager.h"

class HookTemplate
{
public:
static void Command(
    u32 commandType,
    void(*funcPtr)(i64 clientId, const SupervisorMessage& msg, SupervisorSocketManager* managerInstance0)
)
{
    SupervisorGlobal::instance().getSocketManager()->hookCommand(
        commandType, funcPtr);
}

static void OnConnect(
    void(*funcPtr)(i64 clientId, SupervisorSocketManager* managerInstance0)
)
{
    SupervisorGlobal::instance().getSocketManager()->onConnect(funcPtr);
}

static void OnDisconnect(
    void(*funcPtr)(i64 clientId, SupervisorSocketManager* managerInstance0)
)
{
    SupervisorGlobal::instance().getSocketManager()->onDisconnect(funcPtr);
}
};
