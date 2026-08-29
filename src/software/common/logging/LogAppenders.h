// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: LogAppenders.h
 * Purpose: Appenders for logging [how logs are outputted]
 */

class Appender {
public:
    virtual ~Appender() = default;
    virtual void append(const LogEvent& event) = 0;
};

// Console Appender with colored output
class ConsoleAppender : public Appender {
private:
    std::mutex mutex_;
public:
    void append(const LogEvent& event) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // [TIMESTAMP] [LEVEL/V-LEVEL] [LOGGER] (FILE:LINE) = MESSAGE
        auto time_t = std::chrono::system_clock::to_time_t(event.timestamp);
        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", std::localtime(&time_t));

        std::cout << "[" << time_str << "] ["
                  << log_level_to_string(event.level);

        if (event.verbosity > 0) {
            std::cout << ":V" << event.verbosity;
        }

        std::cout << "] [" << event.logger_name << "] "
                  << "(" << event.file << ":" << event.line << ") - "
                  << event.message << "\n";
    }
};
