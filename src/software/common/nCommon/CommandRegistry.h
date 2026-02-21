// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
/* 
 * FileName: CommandRegistry.h
 * Purpose: Stores commands in a memory registry for the host to call commands when the
 * application is requesting for a command.
 */

#pragma once
#include <iostream>
#include <ostream>
#include "nCommon/CommandListCategories.h"
#include "nCommon/Hashing.h"

struct ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void ExecuteStatic(void* context) = 0;
};

class CommandRegistry
{
public:

    static
void ExecuteCommandBuffer(void* context, const uint8_t* data, size_t size)
    {
        size_t offset = 0;

        while (offset < size)
        {
            // Read command header
            SerializedCommandHeader header;
            memcpy(&header, data + offset, sizeof(SerializedCommandHeader));
            offset += sizeof(SerializedCommandHeader);

            // Read payload
            const uint8_t* payload = data + offset;
            offset += header.size;

            // Instantiate from registry
            ICommand* cmd = Instance().Create(header.nameHash, payload);
            if (cmd)
            {
                cmd->ExecuteStatic(context);
                delete cmd;
            }
            else
            {
                printf("Unknown command hash: 0x%llx\n", header.nameHash);
            }
        }
    }


    using FactoryFunc = std::function<ICommand* (const void* payload)>;

    struct Entry {
        FactoryFunc factory;
        size_t typeHash;
    };

    static CommandRegistry& Instance()
    {
        static CommandRegistry instance;
        return instance;
    }

    template <typename Interface = ICommand>
    void Register(uint64_t commandID, FactoryFunc func)
    {
        factories[commandID] = { func, typeid(Interface).hash_code() };
    }

    template <typename Interface = ICommand>
    Interface* Create(uint64_t commandID, const void* payload)
    {
        auto it = factories.find(commandID);
        if (it != factories.end()) {
            if (it->second.typeHash != typeid(Interface).hash_code()) {
                // Error: Type mismatch!
                return nullptr;
            }

            return static_cast<Interface*>(it->second.factory(payload));
        }
        return nullptr;
    }

private:
    std::unordered_map<uint64_t, Entry> factories;
};

inline uint64_t MakeCommandID(const char* category,
                              const char* name)
{
    uint64_t h = BoraHash64(category, strlen(category));
    h = BoraHash64(name, strlen(name), h); // continue hash
    return h;
}

// CommandCategory must be a valid definiton and result in a const char*
// CommandType must derive from ICommand
#define REGISTER_LIST_COMMAND(CommandCategory, CommandType)               \
namespace {                                                               \
struct CommandCategory##CommandType##Register {                       \
CommandCategory##CommandType##Register() {                        \
CommandRegistry::Instance().Register(                         \
MakeCommandID(CommandCategory, #CommandType),                             \
[](const void* payload) -> ICommand* {                   \
return new CommandType(                               \
*static_cast<const CommandType##Data*>(payload)  \
);                                                    \
}                                                         \
);                                                            \
}                                                                 \
};                                                                    \
static CommandCategory##CommandType##Register                          \
s_##CommandCategory##CommandType##Register;                       \
}

struct TestCommand : public ICommand
{
    TestCommandData data;

    TestCommand(const TestCommandData& src)
        : data(src)
    {
    }

    void ExecuteStatic(void* context) override
    {
        std::cout << "Test command executed! Value:" << data.value << std::endl;
    }
};

using namespace CommandCategories;
REGISTER_LIST_COMMAND(NativeTesting, TestCommand);


