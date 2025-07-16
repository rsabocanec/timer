#ifndef SABO_TIMER_H
#define SABO_TIMER_H

#pragma once

#include <functional>
#include <tuple>
#include <chrono>
#include <thread>
#include <atomic>
#include <future>
#include <string_view>
#include <source_location>

namespace std {
class os;
}

namespace rsabo {

template <typename Period>
using duration = std::chrono::duration<int64_t, Period>;

// Helper method for invoking a method in the timer class
template <class Tuple, size_t... Indices>
inline void invoke(Tuple&& func_values, std::index_sequence<Indices...>) noexcept{
    std::invoke(std::move(std::get<Indices>(func_values))...);
}

void report_error(
    std::ostream& os,
    int32_t error_num,
    std::string_view prefix = {},
    std::source_location location = std::source_location::current()) noexcept;

enum class timer_clock_t : int {
    clock_realtime = 0,
    clock_monotonic = 1,
    //clock_process_cputime_id = 2,
    //clock_thread_cputime_id = 3,
    //clock_monotonic_raw = 4,
    //clock_realtime_coarse = 5,
    //clock_monotonic_coarse = 6,
    clock_boottime = 7,
    clock_realtime_alarm = 8,
    clock_boottime_alarm = 9
    //clock_tai = 11
};

class timer {
    timer_clock_t clock_type_{timer_clock_t::clock_monotonic};
    int32_t descriptor_{-1};
    std::atomic<int> error_{};

    public:
    timer() noexcept {
        [[maybe_unused]] auto const result = open();
    }

    explicit timer(timer_clock_t clock_type) noexcept
    : clock_type_(clock_type) {
        [[maybe_unused]] auto const result = open();
    }

    template <typename Period>
    explicit timer( duration<Period>&& interval,
                    timer_clock_t clock_type = timer_clock_t::clock_monotonic) noexcept
    : clock_type_(clock_type) {
        error_ = open();
        if (error_ == 0) {
            error_ = arm(std::forward<duration<Period>>(interval));
        }
    }

    template <typename Period, typename Function, typename... Args>
    explicit timer( duration<Period>&& interval, timer_clock_t clock_type,
                    Function&& func, Args&&... args) noexcept
    : clock_type_(clock_type) {
        error_ = open();
        if (error_ == 0) {
            if (arm(std::forward<duration<Period>>(interval)) == 0) {
                using tuple = std::tuple<std::decay_t<Function>, std::decay_t<Args>...>;
                auto decay_copied = std::make_unique<tuple>(std::forward<Function>(func), std::forward<Args>(args)...);

                std::thread([this](decltype(decay_copied)&& params) {
                    while (!disarmed()) {
                        error_ = wait();
                        if (error_ == 0) {
                            invoke<tuple>(tuple(*params), std::make_index_sequence<1 + sizeof...(Args)>{});
                        }
                        else {
                            break;
                        }
                    }
                }, std::move(decay_copied)).detach();
            }
        }
    }

    template <typename Period, typename Function, typename... Args>
    explicit timer(duration<Period>&& interval, Function&& func, Args&&... args) noexcept
    : timer(std::forward<duration<Period>>(interval),
            timer_clock_t::clock_monotonic,
            std::forward<Function>(func),
            std::forward<Args>(args)...) {
    }

    timer(const timer&) = delete;

    timer(timer&& other) noexcept
    : clock_type_(other.clock_type_) {
        auto const tmp = other.descriptor_;
        other.descriptor_ = -1;
        descriptor_ = tmp;
    }

    timer& operator=(const timer&) = delete;

    timer& operator=(timer&& other) noexcept {
        if (this != &other) {
            auto const tmp = other.descriptor_;
            other.descriptor_ = -1;
            descriptor_ = tmp;

            clock_type_ = other.clock_type_;
        }

        return *this;
    }

    ~timer() {
        [[maybe_unused]] auto const result = close();
    };

    bool valid() const noexcept {
        return descriptor_ >= 0;
    }

    [[nodiscard]] int32_t error() const noexcept {
        return error_;
    }

    template <typename Period>
    [[nodiscard]] int32_t arm(duration<Period>&& interval) noexcept {
        return arm(std::chrono::duration_cast<std::chrono::nanoseconds>(interval).count());
    }

    [[nodiscard]] int32_t disarm() noexcept;
    [[nodiscard]] bool disarmed() noexcept;
    [[nodiscard]] int32_t wait() noexcept;

private:
    [[nodiscard]] int32_t open() noexcept;
    [[nodiscard]] int32_t close() noexcept;

    [[nodiscard]] int32_t arm(int64_t nanoseconds) noexcept;
};
}
#endif //SABO_TIMER_H
