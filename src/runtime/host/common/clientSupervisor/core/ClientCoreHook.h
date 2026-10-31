// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: SupervisorCoreHook.h
 * Purpose: Sets the core commands f
*/
#pragma once
#include "guest/wasm/WasmGlobal.h"
#include "guest/wasm/WasmSupervisorManager.h"
#include "networking/supervisor/codecs/FileDescriptor.h"
#include "networking/supervisor/codecs/Primitives.h"
#include "nThread/ThreadBufferPool.h"

class ClientCoreHook
{
public:
    // Sets the file descriptor for shared wasm memory
    static void get_version(const SupervisorMessage& msg, WasmSupervisorManager* managerInstance)
    {
        vec8& buffer = ThreadBufferPool::getScratchBuffer();
        SupervisorCodecs::String(BORA_VERSION).serialize(buffer);
        managerInstance->reply(msg, buffer.data(), buffer.size());
    }

    static void has_received_filedescriptor(const SupervisorMessage& msg, WasmSupervisorManager* managerInstance)
    {
        // There will be only one global condition variable inside WasmLibraryLinker::instance()->condition being used for the time being.
        // The main thread will block waiting for the condition, while the command thread will tell us the condition.
        WasmGlobal::getInstance()->was_filedescriptor_received.notify_all();
    }

    static void who_am_i(const SupervisorMessage& msg, WasmSupervisorManager* managerInstance)
    {
        vec8& buffer = ThreadBufferPool::getScratchBuffer();
        auto module = WasmGlobal::getInstance()->getMainModule();
        if (!module)
        {
            managerInstance->reply(msg);
            return;
        }
        SupervisorCodecs::String( module->appId).serialize(buffer);
        managerInstance->reply(msg, buffer.data(), buffer.size());
    }

    static void on_disconnect(WasmSupervisorManager* managerInstance)
    {
        if (WasmGlobal::getInstance()->getSupervisor()->isRunning())
        {
            LOG_ERROR(LogManager::instance().getLogger("bora.supervisor")) << "Supervisor is unavailable, it is not safe to continue.";
            _exit(1);
        }
    }

    static void on_connect(WasmSupervisorManager* managerInstance)
    {
        LOG_INFO_VERBOSE(LogManager::instance().getLogger("bora.supervisor"), 1) << "CONNECTED TO SUPERVISOR";
    }
};
