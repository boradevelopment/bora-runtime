// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: Logger.h
 * Purpose: The logging instance where outputs are made.
 */

#pragma once
#include "LogHeaders.h"
#include "LogAppenders.h"

class Logger : public std::enable_shared_from_this<Logger> {
private:
    std::string name_;
    LogLevel level_ = LogLevel::INFO;
    int verbosity_limit_ = 0; // Max verbosity sub-level allowed
    std::shared_ptr<Logger> parent_ = nullptr;
    std::vector<std::shared_ptr<Appender>> appenders_;
    std::mutex mutex_;
    bool additivity_ = true;
public:
    explicit Logger(std::string name) : name_(std::move(name))
    {

    }

    void setLevel(LogLevel level) { level_ = level; }
    LogLevel getLevel() const { return level_; }

    void setVerbosityLimit(int limit) { verbosity_limit_ = limit; }
    int getVerbosityLimit() const { return verbosity_limit_; }

    void set_additivity(bool additivity) { additivity_ = additivity; }
    bool get_additivity() const { return additivity_; }

    void setParent(std::shared_ptr<Logger> parent)
    {
        parent_ = parent;
        setVerbosityLimit(parent_->getVerbosityLimit());
    }

    void addAppender(std::shared_ptr<Appender> appender) {
        std::lock_guard<std::mutex> lock(mutex_);
        appenders_.push_back(appender);
    }

    /// Evaluates if a log message should pass based on Level & Verbosity Layering
    bool isEnabledFor(LogLevel level, int verbosity = 0) const {
        if (level < level_) return false;
        if (verbosity > verbosity_limit_) return false;
        return true;
    }

    /// Dispatches events down through appender inheritance hierarchy (Inspired by Log4J if you couldn't tell)
    void log(LogLevel level, int verbosity, const std::string& msg, const char* file, int line) {
        if (!isEnabledFor(level, verbosity)) return;

        LogEvent event{
            name_,
            level,
            verbosity,
            msg,
            file,
            line,
            std::chrono::system_clock::now()
        };

        // Forward to local appenders and parent appenders
        const Logger* curr = this;
        while (curr != nullptr) {
            for (const auto& appender : curr->appenders_) {
                appender->append(event);
            }

            if (!curr->additivity_) {
                break;
            }

            curr = curr->parent_.get(); // Bubble up hierarchy
        }
    }
};