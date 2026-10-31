// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: HookTemplate.h
 * Purpose: ?
*/
#pragma once
#include "guest/wasm/WasmGlobal.h"
#include "guest/wasm/WasmSupervisorManager.h"
#include "supervisor/SupervisorGlobal.h"

class HookTemplate
{
public:
static void Command(
    u32 commandType,
    void(*funcPtr)(const SupervisorMessage& msg, WasmSupervisorManager* managerInstance0)
)
{
    WasmGlobal::getInstance()->getSupervisor()->hookCommand(commandType, funcPtr);
}

static void OnConnect(
    void(*funcPtr)(WasmSupervisorManager* managerInstance0)
)
{
    WasmGlobal::getInstance()->getSupervisor()->onConnect(funcPtr);
}

static void OnDisconnect(
    void(*funcPtr)(WasmSupervisorManager* managerInstance0)
)
{
    WasmGlobal::getInstance()->getSupervisor()->onDisconnect(funcPtr);
}
};
