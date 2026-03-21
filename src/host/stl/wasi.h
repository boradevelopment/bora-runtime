// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: wasi.h
 * Purpose: WASI Symbols
 */
#pragma once
#include "hostSymbolTemplate.hpp"

#pragma pack(push, 1) // Force no padding
struct ioVectorArray {
    uint64_t buf_ptr;
    uint64_t buf_len;
};
#pragma pack(pop)

class WasiHostSymbols : public HostSymbolTemplate {

    static int32_t fd_seek_handler(wasm_exec_env_t exec_env,
                                  int32_t fd,
                                  int64_t offset,
                                  int32_t whence,
                                  uint64_t newoffset_ptr) {

        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);

        // 1. Translate the guest address to a host pointer
        u64* res_newoffset = (u64*)wasm_runtime_addr_app_to_native(inst, newoffset_ptr);

        if (res_newoffset) {
            // Since we aren't actually tracking file pointers,
            // we'll just return the offset as if we started from 0.
            *res_newoffset = (uint64_t)offset;
        }

        // Return 0 for Success (WASI_ESUCCESS)
        return 0;
    }

    static int32_t fd_write_handler(wasm_exec_env_t exec_env, int32_t fd, uint64_t iov_offset, uint64_t iov_cnt, uint64_t nwritten_offset) {
        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);

        // Get host address for the IO Vectors array [64-bit address in guest memory]
        auto* iovs = (ioVectorArray*)wasm_runtime_addr_app_to_native(inst, iov_offset);

        uint64_t total_written = 0;
        for (uint64_t i = 0; i < iov_cnt; i++) {
            char* str = (char*)wasm_runtime_addr_app_to_native(inst, iovs[i].buf_ptr);
            if (str) {
                std::cout.write(str, iovs[i].buf_len);
                total_written += iovs[i].buf_len;
            }
        }

        // Write back the number of bytes written to nwritten_offset
        uint64_t* nwritten = (uint64_t*)wasm_runtime_addr_app_to_native(inst, nwritten_offset);
        if (nwritten) *nwritten = total_written;

        return 0;
    }

    // fd_read: (param i32 i64 i64 i64) (result i32)
    static int32_t fd_read_handler(wasm_exec_env_t exec_env, int32_t fd, uint64_t iovs_ptr, uint64_t iovs_cnt, uint64_t nread_ptr) {
        // WASI standard: fd 0 is stdin
        if (fd != 0) return 5; // WASI_EBADF

        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);

        // Structure for Memory64 WASI iovecs
        auto* iovs = (ioVectorArray*)wasm_runtime_addr_app_to_native(inst, iovs_ptr);

        u64 total_read = 0;
        u64 bytes = 0;
        for (u64 i = 0; i < iovs_cnt; i++) {
            char* host_buf = (char*)wasm_runtime_addr_app_to_native(inst, iovs[i].buf_ptr);

            if (host_buf && iovs[i].buf_len > 0) {
                // Check how many bytes are actually waiting in the console buffer
                std::streamsize available = std::cin.rdbuf()->in_avail();

                // If nothing is there, we do a single blocking get() or getline
                // to wait for the user to actually type something.
                if (available <= 0) {
                    std::string line;
                    if (std::getline(std::cin, line)) {
                        line += '\n'; // Add the newline back
                        size_t to_copy = std::min((size_t)line.length(), (size_t)iovs[i].buf_len);
                        std::memcpy(host_buf, line.c_str(), to_copy);
                        bytes = to_copy;
                    }
                } else {
                    // 3. If data is already buffered, take only what we can
                    bytes = std::cin.readsome(host_buf, iovs[i].buf_len);
                }

                total_read += bytes;
                if (bytes < iovs[i].buf_len) break;
            }
        }

        // Write the result back to the WASM guest's provided pointer
        u64* nread = (u64*)wasm_runtime_addr_app_to_native(inst, nread_ptr);
        if (nread) {
            *nread = total_read;
        }

        return 0; // WASI_ESUCCESS
    }

    // environ_sizes_get: (param i64 i64) (result i32)
    static int32_t environ_sizes_get_handler(wasm_exec_env_t exec_env, uint64_t env_count_ptr, uint64_t env_buf_size_ptr) {
        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
        uint64_t *count = (uint64_t*)wasm_runtime_addr_app_to_native(inst, env_count_ptr);
        uint64_t *size = (uint64_t*)wasm_runtime_addr_app_to_native(inst, env_buf_size_ptr);

        if (count) *count = 0;
        if (size) *size = 0;
        return 0;
    }

    // environ_get: (param i64 i64) (result i32)
    static int32_t environ_get_handler(wasm_exec_env_t exec_env, uint64_t env_ptr, uint64_t env_buf_ptr) {
        return 0;
    }

    std::vector<NativeSymbol> symbols =
    {
        {"fd_write",  (void*)fd_write_handler, "(iIII)i", nullptr},
{"fd_seek",(void*)fd_seek_handler,  "(iIiI)i",  nullptr},
{"fd_read",  (void*)fd_read_handler, "(iIII)i",  nullptr},
    {"environ_sizes_get", (void*)environ_sizes_get_handler, "(II)i", nullptr},
    {"environ_get",    (void*)environ_get_handler,    "(II)i",nullptr}
    };

public:
    WasiHostSymbols() {

    }

    const char* get_namespace() const override { return "wasi_snapshot_preview1"; }
    const std::vector<NativeSymbol>& get_symbols() const override { return symbols; }
};
