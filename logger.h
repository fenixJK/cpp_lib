#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <type_traits>

namespace cpplib {
    namespace detail {
        template <typename Enum>
        constexpr auto to_underlying(Enum value) noexcept -> std::underlying_type_t<Enum> {
            return static_cast<std::underlying_type_t<Enum>>(value);
        }
    }

    enum class LogLevel : std::uint8_t {
        TRACE = 0,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        CRITICAL
    };

    constexpr bool operator<(LogLevel lhs, LogLevel rhs) noexcept {
        return detail::to_underlying(lhs) < detail::to_underlying(rhs);
    }

    enum class OutputTarget : std::uint8_t {
        NONE     = 0,
        TERMINAL = 1 << 0,
        FILE     = 1 << 1,
        GUI      = 1 << 2
    };

    inline OutputTarget operator|(OutputTarget lhs, OutputTarget rhs) noexcept {
        return static_cast<OutputTarget>(detail::to_underlying(lhs) | detail::to_underlying(rhs));
    }

    inline OutputTarget operator&(OutputTarget lhs, OutputTarget rhs) noexcept {
        return static_cast<OutputTarget>(detail::to_underlying(lhs) & detail::to_underlying(rhs));
    }

    class Logger {
    public:
        Logger(LogLevel level = LogLevel::INFO, OutputTarget targets = OutputTarget::TERMINAL,
        const std::string& file = "")
            : log_level(level), log_targets(targets) {
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
            if (level < log_level) {
                return;
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

        std::string getTimestamp(bool includeSubsecond = false) {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::tm tm_snapshot{};
#if defined(_WIN32)
            localtime_s(&tm_snapshot, &time_t_now);
#else
            localtime_r(&time_t_now, &tm_snapshot);
#endif

            std::ostringstream oss;
            oss << std::put_time(&tm_snapshot, "%Y-%m-%d %H:%M:%S");
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
            return detail::to_underlying(targets & target) != 0;
        }
    };
}
