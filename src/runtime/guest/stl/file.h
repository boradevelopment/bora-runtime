// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* * FileName: file.h
 * Purpose: File symbols
 */
#pragma once
#include "guest/templates/hostSymbolTemplate.hpp"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <random>
#include <sys/mman.h>

#include "guest/templates/LinkerTemplate.h"
#include "guest/wasm/WasmTools.h"
#include "networking/supervisor/codecs/FileSystem.h"
#include "networking/supervisor/codecs/Primitives.h"

class FileHostSymbols : public HostSymbolTemplate {
private:
    static uint8_t* get_native_memory(wasmtime_caller_t* caller, const WasmRuntimeModule* module) {
        return wasmtime_sharedmemory_data(module->sharedMemory);
    }

    // Helper to resolve a 64-bit WASM offset into a real host native address
    static i32 open_file(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* caller, i64 filePath, i64 modeString) {
        u8* mem_base = get_native_memory(caller, module);
        if (!mem_base) return -1;
        auto path = WasmTools::fromWASM<const char*>(threadInstance, module, filePath);
        auto mode = WasmTools::fromWASM<const char*>(threadInstance, module, modeString);
        auto supervisor = WasmGlobal::getInstance()->getSupervisor();
        vec8 pathBuffer;
        SupervisorCodecs::String(path).serialize(pathBuffer);
        SupervisorCodecs::String(mode).serialize(pathBuffer);
        auto message = supervisor->communicate(OpenFile, pathBuffer.data(), pathBuffer.size());
        if (message.payload.empty()) return -1;
        int id;
        if (!BufferReader(message.payload).read(id)) return -1;

        return id;
    }

    static i32 seek_file(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* caller, i32 handle, i64 offset, i32 position) {
        u8* mem_base = get_native_memory(caller, module);
        if (!mem_base) return -1;
        auto supervisor = WasmGlobal::getInstance()->getSupervisor();
        vec8 buffer;
        auto writer = VectorWriter(buffer);
        writer.write(handle);
        writer.write(offset);
        writer.write(position);
        auto message = supervisor->communicate(SeekFile, buffer.data(), buffer.size());
        if (message.payload.empty()) return -1;
        int nOffset;
        BufferReader(message.payload).read(nOffset);
        return nOffset;
    }

    static i32 read_file(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* caller, i32 handle, i64 buf_offset, u64 size, u64 nmemb) {
        u8* mem_base = get_native_memory(caller, module);
        if (!mem_base) return -1;

        auto supervisor = WasmGlobal::getInstance()->getSupervisor();
        vec8 buffer;
        SupervisorCodecs::ReadFile(handle, buf_offset, size, nmemb).serialize(buffer);
        auto message = supervisor->communicate(ReadFile, buffer.data(), buffer.size());
        if (message.payload.empty()) return -1;
        int nOffset;
        BufferReader(message.payload).read(nOffset);
        return nOffset;
    }

    static i32 close_file(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* caller, i32 handle) {
        u8* mem_base = get_native_memory(caller, module);
        if (!mem_base) return -1;
        auto supervisor = WasmGlobal::getInstance()->getSupervisor();
        vec8 buffer;
        VectorWriter(buffer).write(handle);
        auto message = supervisor->communicate(CloseFile, buffer.data(), buffer.size());
        if (message.payload.empty()) return -1;
        int nOffset;
        BufferReader(message.payload).read(nOffset);
        return nOffset;
    }
public:
    FileHostSymbols() = default;

    const char* get_namespace() const override { return "bora::fs"; }

    void bind_symbols(wasmtime_linker_t* linker) const override {
        const char* ns = get_namespace();
        LinkerTemplate::Function(linker, ns, "openFile", open_file);
        LinkerTemplate::Function(linker, ns, "seekFile", seek_file);
        LinkerTemplate::Function(linker, ns, "readFile", read_file);
        LinkerTemplate::Function(linker, ns, "closeFile", close_file);
    }
};
