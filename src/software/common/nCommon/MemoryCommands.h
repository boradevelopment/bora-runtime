// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
/* 
 * FileName: MemoryCommands.h
 * Purpose: ?
 */

#pragma once
#include "nGraphics/GraphicsSystem.h"

struct MemoryCopyCommand : public IGPUCommand
{
    ResourceHandle<void>* dst;
    const void* src;
    size_t srcSize;
    size_t offset;

    MemoryCopyCommand(ResourceHandle<void>* dst, const void* src, size_t srcSize, size_t offset = 0) : dst(dst), src(src), srcSize(srcSize), offset(offset) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        try
        {
            if (!dst || !src || srcSize < 1) return;

            if (!dst || !dst->Get())
                throw std::invalid_argument("MemoryCopyCommand: Destination resource is null.");

            if (!src)
                throw std::invalid_argument("MemoryCopyCommand: Source pointer is null.");

            if (srcSize == 0)
                throw std::invalid_argument("MemoryCopyCommand: Source size is zero.");


            auto* dstBytes = static_cast<uint8_t*>(dst->Get());
            void* dstObj = dstBytes + offset;

            memcpy(dstObj, src, srcSize);
        }
        catch (const std::exception& e)
        {
            // todo: logging
        }
        catch (...) {
            // todo: logging system
        }

    }
};

struct MemoryFreeCommand : public IGPUCommand
{
    ResourceHandle<void>* memory;

    MemoryFreeCommand(ResourceHandle<void>* memory) : memory(memory) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        try
        {
            if (!memory || !memory->Exists()) return;
            free(memory->Get());
            memory->status = DESTROYED;
        }
        catch (const std::exception& e)
        {
            // todo: logging
        }
        catch (...) {
            // todo: logging system
        }

    }
};

struct MemoryAllocateCommand : public IGPUCommand
{
    ResourceHandle<void>* memory;
    size_t size;

    MemoryAllocateCommand(ResourceHandle<void>* memory, size_t size) : memory(memory), size(size) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        try
        {
            if (!memory) return;

            memory->Resolve(malloc(size));
        }
        catch (const std::exception& e)
        {
            // todo: logging
        }
        catch (...) {
            // todo: logging system
        }

    }
};

class MemoryCommands {
public:
    template<CommandListContainer C>
    static void Copy(C* commandList, ResourceHandle<void>* dataPtr, const void* src, size_t srcSize, size_t offset = 0) {
        commandList->push_back(std::make_unique<MemoryCopyCommand>(dataPtr, src, srcSize, offset));
    }
    template<CommandListContainer C>
    static void Free(C* commandList, ResourceHandle<void>* memory) {
        commandList->push_back(std::make_unique<MemoryFreeCommand>(memory));
    }
    template<CommandListContainer C>
    static void Allocate(C* commandList, ResourceHandle<void>* memory, size_t size) {
        commandList->push_back(std::make_unique<MemoryAllocateCommand>(memory, size));
    }
};