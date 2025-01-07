#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <sstream>

namespace cpplib {
    enum class LogLevel {
        TRACE = 1 << 0,
        DEBUG = 1 << 1,
        INFO  = 1 << 2,
        WARN  = 1 << 3,
        ERROR = 1 << 4,
        CRITICAL = 1 << 5
    };

    enum class OutputTarget {
        TERMINAL = 1 << 0,
        FILE = 1 << 1,
        GUI = 1 << 2
    };

    inline LogLevel operator|(LogLevel lhs, LogLevel rhs) {
        return static_cast<LogLevel>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }
    inline LogLevel operator&(LogLevel lhs, LogLevel rhs) {
        return static_cast<LogLevel>(static_cast<int>(lhs) & static_cast<int>(rhs));
    }
    inline OutputTarget operator|(OutputTarget lhs, OutputTarget rhs) {
        return static_cast<OutputTarget>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }
    inline OutputTarget operator&(OutputTarget lhs, OutputTarget rhs) {
        return static_cast<OutputTarget>(static_cast<int>(lhs) & static_cast<int>(rhs));
    }

    class Logger {
    public:
        Logger(LogLevel level = LogLevel::INFO, OutputTarget targets = OutputTarget::TERMINAL,
        const std::string& file = "", bool async = false) : log_level(level), log_targets(targets), Async(async) {
            if (!file.empty()) {
                log_file = std::make_unique<std::ofstream>(file, std::ios::app);
            }
        }

        ~Logger() {
            if (log_file) {
                log_file->close();
            }
        }

        void log(LogLevel level, const std::string& message) {
            if (isSingleLogLevel(log_level)) {
                if (level < log_level) return;
            } else {
                if (!(static_cast<int>(log_level & level))) return;
            }

            std::string log_message = "[" + getTimestamp() + "] [" + levelToString(level) + "] " + message;
            std::lock_guard<std::mutex> lock(log_mutex);

            if (hasTarget(log_targets, OutputTarget::TERMINAL)) {
                std::cout << log_message << std::endl;
            }
            if (log_file && hasTarget(log_targets, OutputTarget::FILE)) {
                *log_file << log_message << std::endl;
            }
            if (hasTarget(log_targets, OutputTarget::GUI)) {
                // Implement GUI logging later
            }
        }

        void trace(const std::string& message) { log(LogLevel::TRACE, message); }
        void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
        void info(const std::string& message) { log(LogLevel::INFO, message); }
        void warn(const std::string& message) { log(LogLevel::WARN, message); }
        void error(const std::string& message) { log(LogLevel::ERROR, message); }
        void critical(const std::string& message) { log(LogLevel::CRITICAL, message); }

        void setLogLevel(LogLevel level) { log_level = level; }

    private:
        LogLevel log_level;
        OutputTarget log_targets;
        std::unique_ptr<std::ofstream> log_file;
        std::mutex log_mutex;
        bool Async;

        std::string getTimestamp(bool includeSubsecond = false) {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::ostringstream oss;
            oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
            if (includeSubsecond) {
                oss << '.' << std::setfill('0') << std::setw(3) << milliseconds.count();
            }
            return oss.str();
        }

        std::string levelToString(LogLevel level) {
            switch (level) {
                case LogLevel::TRACE:    return "TRACE";
                case LogLevel::DEBUG:    return "DEBUG";
                case LogLevel::INFO:     return "INFO";
                case LogLevel::WARN:     return "WARN";
                case LogLevel::ERROR:    return "ERROR";
                case LogLevel::CRITICAL: return "CRITICAL";
                default:                 return "UNKNOWN";
            }
        }

        inline bool hasTarget(OutputTarget targets, OutputTarget target) {
            return static_cast<int>(targets & target);
        }
        inline bool isSingleLogLevel(LogLevel level) {
            return (static_cast<int>(level) & (static_cast<int>(level) - 1)) == 0;
        }
    };
}