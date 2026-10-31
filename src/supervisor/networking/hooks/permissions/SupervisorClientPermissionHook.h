// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: SupervisorCoreHook.h
 * Purpose: Sets the core commands f
*/
#pragma once
#include "supervisor/networking/hooks/HookTemplate.h"
#include "networking/supervisor/hooks/SupervisorHookRegister.h"
#include "nThread/ThreadBufferPool.h"

class SupervisorClientPermissionHook
{
public:
    // Sets the file descriptor for shared wasm memory
    static void get_permission(i64 clientId, const SupervisorMessage& msg, SupervisorSocketManager* managerInstance)
    {
        AppPermissions permissions; // default for now - We will need to get this from a protected filesystem
        permissions.allowManualInput = true;

        vec8& permissionData = ThreadBufferPool::getScratchBuffer();
        permissions.serialize(permissionData); // send the data
        managerInstance->reply(msg, clientId, permissionData.data(), permissionData.size());
    }
};

