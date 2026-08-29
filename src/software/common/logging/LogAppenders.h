// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: LogAppenders.h
 * Purpose: Appenders for logging [how logs are outputted]
 */
#pragma once
#include "LogHeaders.h"

class Appender {
public:
    virtual ~Appender() = default;
    virtual void append(const LogEvent& event) = 0;
};

namespace LogColor {
    constexpr const char* RESET     = "\033[0m";
    constexpr const char* GRAY      = "\033[90m";
    constexpr const char* CYAN      = "\033[36m";
    constexpr const char* GREEN     = "\033[32m";
    constexpr const char* YELLOW    = "\033[33m";
    constexpr const char* RED       = "\033[31m";
    constexpr const char* BOLD_RED  = "\033[1;31m";
    constexpr const char* BOLD      = "\033[1m";
}

// Console Appender with colored output
class ConsoleAppender : public Appender {
private:
    std::mutex mutex_;

    constexpr const char* getLogLevelColor(LogLevel level) {
        switch (level) {
        case LogLevel::TRACE: return LogColor::GRAY;
        case LogLevel::DEBUG: return LogColor::CYAN;
        case LogLevel::INFO:  return LogColor::GREEN;
        case LogLevel::WARN:  return LogColor::YELLOW;
        case LogLevel::ERR:   return LogColor::RED;
        case LogLevel::FATAL: return LogColor::BOLD_RED;
        default:              return LogColor::RESET;
        }
    }

public:
    void append(const LogEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // [TIMESTAMP]
        auto time_t = std::chrono::system_clock::to_time_t(event.timestamp);
        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", std::localtime(&time_t));

        std::cout << LogColor::GRAY << "[" << time_str << "] " << LogColor::RESET;
        const char* level_color = getLogLevelColor(event.level);
        std::cout << level_color << "[" << logLevelToString(event.level);
        if (event.verbosity > 0) {
            std::cout << ":V" << event.verbosity;
        }
        std::cout << "] " << LogColor::RESET;
        std::cout << LogColor::CYAN << "[" << event.logger_name << "] " << LogColor::RESET;

#ifndef NDEBUG
        std::cout << LogColor::GRAY << "(" << event.file << ":" << event.line << ") " << LogColor::RESET
                  << event.message << "\n";
#else
        std::cout << event.message << "\n";
#endif
    }
};

class FileAppender : public Appender {
private:
    std::ofstream file_;
    std::mutex mutex_;

public:
    explicit FileAppender(const std::string& filepath, bool append = true) {
        // Ensure parent directory exists before opening
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }

        auto open_mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
        file_.open(filepath, open_mode);

        if (!file_.is_open()) {
            std::cerr << "[LOG4BORA_ERROR] Failed to open log file: " << filepath << "\n";
        }
    }

    ~FileAppender() override {
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
    }

    void append(const LogEvent& event) override {
        if (!file_.is_open()) return;

        std::lock_guard<std::mutex> lock(mutex_);

        // Format timestamp with date for file records
        auto time_t = std::chrono::system_clock::to_time_t(event.timestamp);
        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));

        file_ << "[" << time_str << "] ["
              << logLevelToString(event.level);

        if (event.verbosity > 0) {
            file_ << ":V" << event.verbosity;
        }

#ifndef NDEBUG
        file_ << "] [" << event.logger_name << "] "
            << "(" << event.file << ":" << event.line << ") - "
            << event.message << "\n";
#else
        file_ << "] [" << event.logger_name << "] - "
                 << event.message << "\n";
#endif

        // Immediately flush WARN, ERROR, and FATAL logs to disk to prevent loss during crashes
        if (event.level >= LogLevel::WARN) {
            file_.flush();
        }
    }
};