// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* * FileName: wasi.h
 * Purpose: WASI Symbols (Memory64 Aligned for Wasmtime)
 */
#pragma once
#include "hostSymbolTemplate.hpp"
#include <cstring>
#include <algorithm>
#include <iostream>

#pragma pack(push, 1) // Force no padding
struct ioVectorArray {
    uint64_t buf_ptr;
    uint64_t buf_len;
};

// WASI fdstat structure (Memory64 aligned)
struct wasi_fdstat_t {
    uint8_t fs_filetype;
    uint16_t fs_flags;
    uint64_t fs_rights_base;
    uint64_t fs_rights_inheriting;
};

// WASI prestat structure (Memory64 aligned)
struct wasi_prestat_t {
    uint8_t pr_type; // 0 is WASI_PREOPENTYPE_DIR
    uint64_t pr_name_len;
};
#pragma pack(pop)

class WasiHostSymbols : public HostSymbolTemplate {
private:
    // Helper to resolve a 64-bit WASM offset into a real host native address
    static uint8_t* get_native_memory(wasmtime_caller_t* caller, wasmtime_context_t* context) {
        wasmtime_extern_t item;
        // Wasm modules export their main linear memory pool as "memory"
        bool found = wasmtime_caller_export_get(caller, "memory", strlen("memory"), &item);

        if (!found || item.kind != WASMTIME_EXTERN_MEMORY) {
            return nullptr;
        }

        // Return the raw underlying host memory address slice
        return wasmtime_memory_data(context, &item.of.memory);
    }

    static int32_t args_sizes_get_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, uint64_t argc_ptr, uint64_t argv_buf_size_ptr) {
        uint8_t* mem_base = get_native_memory(caller, context);
        if (!mem_base) return 21; // WASI_EFAULT

        uint64_t* argc = reinterpret_cast<uint64_t*>(mem_base + argc_ptr);
        uint64_t* argv_buf_size = reinterpret_cast<uint64_t*>(mem_base + argv_buf_size_ptr);

        if (argc) *argc = 0;
        if (argv_buf_size) *argv_buf_size = 0;

        return 0; // WASI_ESUCCESS
    }

    // WASI args_get (Memory64 pointers: uint64_t)
    static int32_t args_get_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, uint64_t argv_ptr, uint64_t argv_buf_ptr) {
        return 0; // WASI_ESUCCESS
    }

    static int32_t fd_seek_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, int32_t fd, int64_t offset, int32_t whence, uint64_t newoffset_ptr) {
        uint8_t* mem_base = get_native_memory(caller, context);
        if (!mem_base) return 21; // WASI_EFAULT

        uint64_t* res_newoffset = reinterpret_cast<uint64_t*>(mem_base + newoffset_ptr);
        if (res_newoffset) {
            *res_newoffset = static_cast<uint64_t>(offset);
        }
        return 0; // WASI_ESUCCESS
    }

    static int32_t fd_write_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, int32_t fd, uint64_t iov_offset, uint64_t iov_cnt, uint64_t nwritten_offset) {
        uint8_t* mem_base = get_native_memory(caller, context);
        if (!mem_base) return 21; // WASI_EFAULT

        auto* iovs = reinterpret_cast<ioVectorArray*>(mem_base + iov_offset);
        uint64_t total_written = 0;

        for (uint64_t i = 0; i < iov_cnt; i++) {
            char* str = reinterpret_cast<char*>(mem_base + iovs[i].buf_ptr);
            if (str) {
                std::cout.write(str, iovs[i].buf_len);
                total_written += iovs[i].buf_len;
            }
        }

        uint64_t* nwritten = reinterpret_cast<uint64_t*>(mem_base + nwritten_offset);
        if (nwritten) *nwritten = total_written;

        return 0; // WASI_ESUCCESS
    }

    static int32_t fd_read_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, int32_t fd, uint64_t iovs_ptr, uint64_t iovs_cnt, uint64_t nread_ptr) {
        if (fd != 0) return 8; // WASI_EBADF (Bad file descriptor)

    uint8_t* mem_base = get_native_memory(caller, context);
    if (!mem_base) return 21; // WASI_EFAULT

    auto* iovs = reinterpret_cast<ioVectorArray*>(mem_base + iovs_ptr);
    uint64_t total_read = 0;

    for (uint64_t i = 0; i < iovs_cnt; i++) {
        char* host_buf = reinterpret_cast<char*>(mem_base + iovs[i].buf_ptr);
        uint64_t requested_bytes = iovs[i].buf_len;

        if (host_buf && requested_bytes > 0) {
            uint64_t bytes_read_for_this_iovec = 0;

            // Read from host std::cin byte-by-byte until we satisfy this vector chunk
            while (bytes_read_for_this_iovec < requested_bytes) {
                // Peek at the stream. If it's EOF or empty, std::cin.get() will naturally block the host thread until input arrives
                int c = std::cin.get();

                if (c == EOF) {
                    if (bytes_read_for_this_iovec > 0) {
                        break; // Return what we got so far
                    }
                    // If we read absolutely nothing and hit EOF, return it to guest
                    uint64_t* nread = reinterpret_cast<uint64_t*>(mem_base + nread_ptr);
                    if (nread) *nread = 0;
                    return 0;
                }

                host_buf[bytes_read_for_this_iovec] = static_cast<char>(c);
                bytes_read_for_this_iovec++;

                // If the user pressed enter, stop reading for this WASI cycle
                // to let the guest process the newline event
                if (c == '\n') {
                    break;
                }
            }

            total_read += bytes_read_for_this_iovec;

            // If we broke early because of a newline or EOF, stop filling subsequent iovecs
            if (bytes_read_for_this_iovec < requested_bytes) {
                break;
            }
        }
    }

    uint64_t* nread = reinterpret_cast<uint64_t*>(mem_base + nread_ptr);
    if (nread) {
        *nread = total_read;
    }

    return 0; // WASI_ESUCCESS
    }

    static int32_t fd_fdstat_get_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, int32_t fd, uint64_t fdstat_ptr) {
        uint8_t* mem_base = get_native_memory(caller, context);
        if (!mem_base) return 21; // WASI_EFAULT

        auto* stat = reinterpret_cast<wasi_fdstat_t*>(mem_base + fdstat_ptr);
        if (!stat) return 21; // WASI_EFAULT

        if (fd >= 0 && fd <= 2) {
            stat->fs_filetype = 2; // WASI_FILETYPE_CHARACTER_DEVICE
            stat->fs_flags = 0;
            stat->fs_rights_base = 0x3FFULL; // Basic read/write rights
            stat->fs_rights_inheriting = 0;
            return 0; // WASI_ESUCCESS
        }

        return 8; // WASI_EBADF
    }

    static int32_t fd_prestat_get_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, int32_t fd, uint64_t prestat_ptr) {
        return 8; // WASI_EBADF
    }

    static int32_t fd_prestat_dir_name_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, int32_t fd, uint64_t path_ptr, uint64_t path_len) {
        return 8; // WASI_EBADF
    }

    static int32_t environ_sizes_get_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, uint64_t env_count_ptr, uint64_t env_buf_size_ptr) {
        uint8_t* mem_base = get_native_memory(caller, context);
        if (!mem_base) return 21; // WASI_EFAULT

        uint64_t* count = reinterpret_cast<uint64_t*>(mem_base + env_count_ptr);
        uint64_t* size = reinterpret_cast<uint64_t*>(mem_base + env_buf_size_ptr);

        if (count) *count = 0;
        if (size) *size = 0;
        return 0;
    }

    static int32_t environ_get_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, uint64_t env_ptr, uint64_t env_buf_ptr) {
        return 0;
    }

    static int32_t clock_time_get_handler(wasmtime_caller_t* caller, wasmtime_context_t* context, int32_t id, uint64_t precision, uint64_t time_ptr) {
        uint8_t* mem_base = get_native_memory(caller, context);
        if (!mem_base) return 21; // WASI_EFAULT

        // Fetch monotonic system clock ticks
        uint64_t timestamp_nanoseconds = 0;

        // WASI Clock IDs: 0 = realtime, 1 = monotonic
        if (id == 1) {
            auto now = std::chrono::steady_clock::now().time_since_epoch();
            timestamp_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        } else {
            auto now = std::chrono::system_clock::now().time_since_epoch();
            timestamp_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        }

        // WASI expects the timestamp written as an opaque u64 value into the linear memory location
        uint64_t* result_destination = reinterpret_cast<uint64_t*>(mem_base + time_ptr);
        if (result_destination) {
            *result_destination = timestamp_nanoseconds;
        }

        return 0; // WASI_ESUCCESS
    }

public:
    WasiHostSymbols() = default;

    const char* get_namespace() const override { return "wasi_snapshot_preview1"; }

    void bind_symbols(wasmtime_linker_t* linker) const override {
        const char* ns = get_namespace();

        LinkerTemplate::Function(linker, ns, "args_sizes_get", args_sizes_get_handler);
        LinkerTemplate::Function(linker, ns, "args_get", args_get_handler);
        LinkerTemplate::Function(linker, ns, "fd_write", fd_write_handler);
        LinkerTemplate::Function(linker, ns, "fd_seek", fd_seek_handler);
        LinkerTemplate::Function(linker, ns, "fd_read", fd_read_handler);
        LinkerTemplate::Function(linker, ns, "fd_fdstat_get", fd_fdstat_get_handler);
        LinkerTemplate::Function(linker, ns, "fd_prestat_get", fd_prestat_get_handler);
        LinkerTemplate::Function(linker, ns, "fd_prestat_dir_name", fd_prestat_dir_name_handler);
        LinkerTemplate::Function(linker, ns, "environ_sizes_get", environ_sizes_get_handler);
        LinkerTemplate::Function(linker, ns, "environ_get", environ_get_handler);
        LinkerTemplate::Function(linker, ns, "clock_time_get", clock_time_get_handler);
    }
};
