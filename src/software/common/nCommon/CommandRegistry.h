// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
/* 
 * FileName: CommandRegistry.h
 * Purpose: Stores commands in a memory registry for the host to call commands when the
 * application is requesting for a command.
 */

#pragma once
#include <utility>

#include "wasmtime.hh"
#include "nCommon/CommandListCategories.h"
#include "nCommon/Hashing.h"
#include "host/WasmTools.h"

struct ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void ExecuteStatic(void* context, wasmtime_context_t* wasmContext = nullptr, const WasmRuntimeModule* wasmModule = nullptr) = 0;
};

class CommandRegistry
{
public:

    static
void ExecuteCommandBuffer(wasmtime_context_t* moduleContext, const WasmRuntimeModule* wasmModule, void* context, const uint8_t* data, u64 size)
    {
         auto logger =  LogManager::instance().getLogger("bora.commands");
        u64 offset = 0;

        while (offset < size)
        {
            // Read command header
            SerializedCommandHeader header{};
            memcpy(&header, data + offset, sizeof(SerializedCommandHeader));
            offset += sizeof(SerializedCommandHeader);
            if (size - offset < header.size)
            {
                LOG_ERROR_VERBOSE(logger, 1) << "Payload size (0x" << header.size << ") out of bounds";
                break;
            }

            // Read payload
            const uint8_t* payload = data + offset;
            offset += header.size;

            // Instantiate from registry
            if (ICommand* cmd = Instance().Create(header.nameHash, payload))
            {
                cmd->ExecuteStatic(context, moduleContext, wasmModule);
                delete cmd;
            }
            else
            {
                LOG_ERROR_VERBOSE(logger, 1) << "Unknown hash of 0x" << header.nameHash;
            }
        }
    }


    using FactoryFunc = std::function<ICommand* (const void* payload)>;

    struct Entry {
        FactoryFunc factory;
        size_t typeHash{};
    };

    static CommandRegistry& Instance()
    {
        static CommandRegistry instance;
        return instance;
    }

    template <typename Interface = ICommand>
    void Register(uint64_t commandID, FactoryFunc func)
    {
        factories[commandID] = { std::move(func), typeid(Interface).hash_code() };
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

    void ExecuteStatic(void* context, wasmtime_context_t* moduleContext, const WasmRuntimeModule* moduleInstance) override
    {
        if (!moduleInstance || !moduleContext) return;
        data.text = WasmTools::fromWASM<const char*>(moduleContext, moduleInstance, reinterpret_cast<u64>(data.text));
        // data.ints = WasmTools::fromWASM<int*>(moduleInstance, (u64)data.ints);
        // std::cout << "Test command executed! Value:" << data.value << " | Text = "<< data.text << std::endl;
        // for (int* p = data.ints; p < (data.ints + data.intsSize); ++p) {
        //     int val = *p;
        //     std::cout << "Int: " << val << std::endl;
        // }
        // std::cout << data.ints[0] << std::endl;
        // std::cout << data.ints[1] << std::endl;

    }
};

using namespace CommandCategories;
REGISTER_LIST_COMMAND(NativeTesting, TestCommand);


