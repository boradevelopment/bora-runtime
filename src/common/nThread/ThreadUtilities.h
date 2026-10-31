// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: ThreadUtilities.h
 * Purpose: Utilities for Threads
*/
#pragma once
#include <string>
#include <thread>

#if defined(_MSC_VER)
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <pthread.h>
#endif

namespace bora::thread {
/// Sets the name of the current running thread for debugging and profiling tools.
inline void nameThread(const char* name) {
    if (!name) return;

#if defined(_MSC_VER)
    // Windows implementation (requires Windows 10 1607+ or VS 2019+)
    // Convert const char* to wchar_t*
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if (sizeNeeded > 0) {
        std::wstring wname(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, name, -1, &wname[0], sizeNeeded);
        SetThreadDescription(GetCurrentThread(), wname.c_str());
    }
#elif defined(__APPLE__)
    // macOS implementation (limited to 64 characters including null terminator)
    pthread_setname_np(name);
#elif defined(__linux__) || defined(__ANDROID__)
    // Linux implementation (limited to 16 characters including null terminator)
    pthread_setname_np(pthread_self(), name);
#endif
}

/// Sets the name of an external thread handle (where supported by the platform).
inline void nameThread(std::thread& thread, const char* name) {
    if (!name) return;

#if defined(_MSC_VER)
    // Windows supports setting other thread descriptions via native handle
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    if (sizeNeeded > 0) {
        std::wstring wname(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, name, -1, &wname[0], sizeNeeded);
        SetThreadDescription(thread.native_handle(), wname.c_str());
    }
#elif defined(__APPLE__) || defined(__linux__) || defined(__ANDROID__)
    pthread_setname_np(thread.native_handle(), name);
#endif
}

} // namespace bora::thread
