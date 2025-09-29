#pragma once

#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <thread>

namespace cpplib {
    class Stopwatch {
    public:
        using clock = std::chrono::steady_clock;

        Stopwatch() : start_(clock::now()) {}

        void reset() {
            start_ = clock::now();
        }

        template <typename Duration = std::chrono::milliseconds>
        Duration elapsed() const {
            return std::chrono::duration_cast<Duration>(clock::now() - start_);
        }

    private:
        clock::time_point start_;
    };

    class ScopedTimer {
    public:
        using Callback = std::function<void(std::string_view, std::chrono::nanoseconds)>;

        explicit ScopedTimer(Callback callback) : callback_(std::move(callback)) {}

        ScopedTimer(std::string label, Callback callback = {})
            : label_(std::move(label)), callback_(std::move(callback)) {}

        ScopedTimer(ScopedTimer&&) noexcept = default;
        ScopedTimer& operator=(ScopedTimer&&) noexcept = default;

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;

        ~ScopedTimer() {
            const auto duration = watch_.elapsed<std::chrono::nanoseconds>();
            if (callback_) {
                callback_(label_, duration);
            } else if (!label_.empty()) {
                auto millis = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
                std::cout << label_ << " took " << millis << " ms" << std::endl;
            }
        }

    private:
        Stopwatch watch_{};
        std::string label_;
        Callback callback_{};
    };
    template <typename Duration>
    void hypersleep(Duration duration) {
        using steady_duration = std::chrono::steady_clock::duration;
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::duration_cast<steady_duration>(duration);

        const auto sleep_guard = std::chrono::microseconds(50);
        const auto minimal_sleep = std::chrono::microseconds(5);

        while (true) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                break;
            }

            const auto remaining = deadline - now;
            if (remaining > sleep_guard) {
                auto sleep_for = remaining - sleep_guard;
                if (sleep_for < minimal_sleep) {
                    sleep_for = minimal_sleep;
                }
                std::this_thread::sleep_for(sleep_for);
            } else {
                std::this_thread::yield();
            }
        }
    }
}
