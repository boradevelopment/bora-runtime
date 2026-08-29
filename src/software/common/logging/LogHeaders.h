// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
/* 
 * FileName: LogHeaders.h
 * Purpose: Core stuff for Logging.
 */
#pragma once
#include <string>
#include <chrono>
#include <sstream>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <filesystem>

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERR   = 4,
    FATAL = 5,
    OFF   = 6
};

// Log Context Event passed through the appender pipeline
struct LogEvent {
    std::string logger_name;
    LogLevel level;
    int verbosity; // Layered verbosity sub-level (0 = standard, 1-5 = detailed debug)
    std::string message;
    std::string file;
    int line;
    std::chrono::system_clock::time_point timestamp;
};

inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERR:   return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}