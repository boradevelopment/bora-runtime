// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: SupervisorCoreHook.h
 * Purpose: Sets the core commands f
*/
#pragma once
#include "networking/supervisor/codecs/FileDescriptor.h"
#include "networking/supervisor/codecs/Primitives.h"
#include "supervisor/networking/hooks/HookTemplate.h"
#include "networking/supervisor/hooks/SupervisorHookRegister.h"
#include "nThread/ThreadBufferPool.h"

class SupervisorCoreHook
{
public:
    // Sets the file descriptor for shared wasm memory
    static void set_wasm_fd(i64 clientId, const SupervisorMessage& msg, SupervisorSocketManager* managerInstance)
    {
        // there will be two different types of file descriptors sent.
        // 0 - Shared Memory FD
        // 1 - TAZA Archive FD - requires an ID inside the message.

        auto client = SupervisorGlobal::instance().getClient(clientId);
        auto session = managerInstance->getSession(clientId);
        if (!client || !session) return;

        auto& buffer = msg.payload;
        auto reader = BufferReader(buffer);
        u32 type;
        if (!reader.read(type))
        {
            vec8& nBuffer = ThreadBufferPool::getScratchBuffer();
            SupervisorCodecs::String("INVALID PAYLOAD").serialize(nBuffer);
            managerInstance->reply(msg, clientId, nBuffer.data(), nBuffer.size());
            return;
        }
        if (type == SupervisorCodecs::TAZAArchiveMapping)
        {
            auto fileDescriptorInfo = SupervisorCodecs::GeneralFileDescriptor<SupervisorCodecs::TAZAArchiveMappingFileDescriptor>(msg.payload);
            if (!fileDescriptorInfo.fileDescriptor)
            {
                vec8& nBuffer = ThreadBufferPool::getScratchBuffer();
                SupervisorCodecs::String("INVALID PAYLOAD").serialize(nBuffer);
                managerInstance->reply(msg, clientId, nBuffer.data(), nBuffer.size());
                return;
            }

            // Get the name
            auto future = managerInstance->reply(msg, clientId);
            future.wait(); // Wait for it to get sent.

            session->disableReading = true;
            while (!session->hasStoppedReading)
            {
                // todo.
            }
            managerInstance->sendMessage(clientId, FileDescriptorNotifier);
            int fd = managerInstance->getSocket(clientId)->receive_fd();
            session->disableReading = false;

            auto& id = fileDescriptorInfo.fileDescriptor->appId;
            if (client->getAppId().empty())
            {
                client->setAppId(id);
            }

            if (id == client->getAppId())
            {
                client->setFileDescriptorArchive(fd);
            } else
            {
                client->addDependency(id, fd);
            }
        } else
        {
            auto future = managerInstance->reply(msg, clientId);
            future.wait();
            session->disableReading = true;
            while (!session->hasStoppedReading)
            {
                // todo.
            }
            managerInstance->sendMessage(clientId, FileDescriptorNotifier);
            int fd = managerInstance->getSocket(clientId)->receive_fd();
            session->disableReading = false;
            client->setFileDescriptor(fd);
        }

        managerInstance->sendMessage(clientId, FileDescriptorNotifier);
    }

    static void ping(i64 clientId, const SupervisorMessage& msg, SupervisorSocketManager* managerInstance)
    {
        managerInstance->reply(msg, clientId); // reply back, we can measure it using communicate(
    }

    static void on_disconnect(i64 clientId, SupervisorSocketManager* managerInstance)
    {
        SupervisorGlobal::instance().removeClient(clientId);
    }

    static void on_connect_init(i64 clientId, SupervisorSocketManager* managerInstance)
    {
       // We need to confirm if the supervisor and client can collaborate properly
        const char* version = BORA_VERSION;
        auto clientVersionMessage = managerInstance->communicate(clientId, GetVersion);
        SupervisorCodecs::String cliVersion(clientVersionMessage.payload);
        if (*cliVersion != version)
        {
            managerInstance->getSocket(clientId)->close();
        } else
        {
            SupervisorGlobal::instance().addClient(clientId);
        }
    }
};
