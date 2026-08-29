// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: LogStream.h
 * Purpose: For streaming output to a logger.
 */

#pragma once
#include "LogHeaders.h"

consteval static const char* trim_file_path(const char* file) {
    if (!file) return "";

    std::string_view sv(file);

    auto pos = sv.find("\\src\\");
    if (pos != std::string_view::npos) {
        return file + pos + 5;
    }

    pos = sv.find("/src/");
    if (pos != std::string_view::npos) {
        return file + pos + 5;
    }

    return file;
}

class LogStream {
private:
    std::shared_ptr<Logger> logger_;
    LogLevel level_;
    int verbosity_;
    const char* file_;
    int line_;
    std::ostringstream stream_;

public:
    LogStream(std::shared_ptr<Logger> logger, LogLevel level, int verbosity, const char* file, int line)
        : logger_(logger), level_(level), verbosity_(verbosity), file_(file), line_(line) {}

    ~LogStream() {
        if (logger_) {
            // shorten file to relative rather than full from the src directory
            logger_->log(level_, verbosity_, stream_.str(), file_, line_);
        }
    }

    template <typename T>
    LogStream& operator<<(const T& val) {
        stream_ << val;
        return *this;
    }
};

// macros with file/line information
#ifndef NDEBUG
#define BORA_LOG_LEVEL(logger, level, verbosity) \
if (logger->isEnabledFor(level, verbosity)) \
LogStream(logger, level, verbosity, trim_file_path(__FILE__), __LINE__)
#else
#define BORA_LOG_LEVEL(logger, level, verbosity) \
if (logger->isEnabledFor(level, verbosity)) \
LogStream(logger, level, verbosity, "", 0)
#endif

// macros for convenience
#define LOG_INFO(logger)           BORA_LOG_LEVEL(logger, LogLevel::INFO, 0)
#define LOG_WARN(logger)           BORA_LOG_LEVEL(logger, LogLevel::WARN, 0)
#define LOG_ERROR(logger)          BORA_LOG_LEVEL(logger, LogLevel::ERR, 0)
#define LOG_TRACE(logger)          BORA_LOG_LEVEL(logger, LogLevel::TRACE, 0)
#define LOG_INFO_VERBOSE(logger, verbosity)           BORA_LOG_LEVEL(logger, LogLevel::INFO, verbosity)
#define LOG_WARN_VERBOSE(logger, verbosity)           BORA_LOG_LEVEL(logger, LogLevel::WARN, verbosity)
#define LOG_ERROR_VERBOSE(logger, verbosity)          BORA_LOG_LEVEL(logger, LogLevel::ERR, verbosity)
#define LOG_TRACE_VERBOSE(logger, verbosity)          BORA_LOG_LEVEL(logger, LogLevel::TRACE, verbosity)