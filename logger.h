#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <memory>

namespace cpplib {
    enum class LogLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    enum class OutputTarget {
        TERMINAL = 1 << 0,
        FILE = 1 << 1,
        GUI = 1 << 2
    };

    class Logger {
    public:
        Logger(LogLevel level = LogLevel::INFO, OutputTarget targets = OutputTarget::TERMINAL,
        const std::string& file = "") : log_level(level), log_targets(targets) {
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
            if (level < log_level) return;

            std::lock_guard<std::mutex> lock(log_mutex);
            std::string log_message = "[" + getTimestamp() + "] [" + levelToString(level) + "] " + message;

            if (static_cast<int>(log_targets) & static_cast<int>(OutputTarget::TERMINAL)) {
                std::cout << log_message << std::endl;
            }
            if (log_file && (static_cast<int>(log_targets) & static_cast<int>(OutputTarget::FILE))) {
                *log_file << log_message << std::endl;
            }
            if (static_cast<int>(log_targets) & static_cast<int>(OutputTarget::GUI)) {
                // will implement at a later point
            }
        }

        void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
        void info(const std::string& message) { log(LogLevel::INFO, message); }
        void warn(const std::string& message) { log(LogLevel::WARN, message); }
        void error(const std::string& message) { log(LogLevel::ERROR, message); }

        void setLogLevel(LogLevel level) { log_level = level; }

    private:
        LogLevel log_level;
        OutputTarget log_targets;
        std::unique_ptr<std::ofstream> log_file;
        std::mutex log_mutex;

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
                case LogLevel::DEBUG: return "DEBUG";
                case LogLevel::INFO:  return "INFO";
                case LogLevel::WARN:  return "WARN";
                case LogLevel::ERROR: return "ERROR";
                default:              return "UNKNOWN";
            }
        }
    };
}