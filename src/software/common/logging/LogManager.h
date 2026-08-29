// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: LogManager.h
 * Purpose: ?
 */

#pragma once
#include "LogHeaders.h"
#include "Logger.h"
#include "LogAppenders.h"
#include "tools/AppParam.h"

REGISTER_PARAM_A("verbosity", {"-v"})

class LogManager {
    std::shared_ptr<Logger> root_logger_;
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    std::mutex mutex_;

    LogManager() { // file appenders will be registered through a parameter in main.
        root_logger_ = std::make_shared<Logger>("root");
        root_logger_->addAppender(std::make_shared<ConsoleAppender>());
    }

    std::shared_ptr<Logger> getLoggerUnlocked(const std::string& name) {
        if (name.empty() || name == "root") return root_logger_;

        auto it = loggers_.find(name);
        if (it != loggers_.end()) return it->second;

        // Create new logger and assign parent based on dot-notation hierarchy
        auto newLogger = std::make_shared<Logger>(name);

        // Find parent (e.g. "bora.module" -> "bora" -> root)
        size_t lastDot = name.find_last_of('.');
        if (lastDot != std::string::npos) {
            std::string parent_name = name.substr(0, lastDot);
            newLogger->setParent(getLoggerUnlocked(parent_name));
        } else {
            newLogger->setParent(root_logger_);
        }

        loggers_[name] = newLogger;
        return newLogger;
    }

public:
    static LogManager& instance() {
        static LogManager mgr;
        return mgr;
    }

    std::shared_ptr<Logger> getLogger(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return getLoggerUnlocked(name);
    }
};