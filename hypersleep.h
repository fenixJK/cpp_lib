#pragma once
#include <chrono>
#include <thread>
#include <functional>

namespace cpplib {
    template <typename Duration>
    void hypersleep(Duration duration) {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = start + duration;
        while (std::chrono::high_resolution_clock::now() < end) {
            auto remaining = std::chrono::duration_cast<Duration>(end - std::chrono::high_resolution_clock::now());
            if (remaining > std::chrono::duration_cast<Duration>(std::chrono::microseconds(30))) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<long long>(remaining.count() * 0.7)));
            }
        }
    }

    template <typename Duration, typename Func, typename... Args>
    auto timeFunction(Func&& func, Args&&... args) {
        static_assert(std::is_same_v<Duration, std::chrono::nanoseconds> || 
                        std::is_same_v<Duration, std::chrono::microseconds> || 
                        std::is_same_v<Duration, std::chrono::milliseconds> || 
                        std::is_same_v<Duration, std::chrono::seconds> ||
                        std::is_same_v<Duration, std::chrono::minutes> ||
                        std::is_same_v<Duration, std::chrono::hours>,
                        "Invalid time unit. Use a valid std::chrono duration type.");
        auto start = std::chrono::high_resolution_clock::now();
        std::forward<Func>(func)(std::forward<Args>(args)...);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<Duration>(end - start).count();
    }
}