/*
Copyright (c) 2025 robert.sabocanec@gmail.com

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef RSABO_TIMER_H
#define RSABO_TIMER_H

#pragma once

#include <functional>
#include <tuple>
#include <chrono>
#include <thread>
#include <future>
#include <shared_mutex>
#include <string_view>
#include <tuple>
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
protected:
    timer_clock_t clock_type_{timer_clock_t::clock_monotonic};
    int32_t descriptor_{-1};
    mutable std::shared_mutex mutex_{};

    public:
    timer() noexcept {
        [[maybe_unused]] auto const result = open();
    }

    explicit timer(timer_clock_t clock_type) noexcept
    : clock_type_(clock_type) {
        [[maybe_unused]] auto const result = open();
    }

    timer(const timer&) = delete;

    timer(timer&& other) noexcept
    : clock_type_(other.clock_type_) {
        std::unique_lock lock(mutex_);
        const int32_t tmp = other.descriptor_;
        other.descriptor_ = -1;
        descriptor_ = tmp;

        clock_type_ = other.clock_type_;
    }

    timer& operator=(const timer&) = delete;

    timer& operator=(timer&& other) noexcept {
        if (this != &other) {
            std::unique_lock lock(mutex_);
            const int32_t tmp = other.descriptor_;
            other.descriptor_ = -1;
            descriptor_ = tmp;

            clock_type_ = other.clock_type_;
        }

        return *this;
    }

    virtual ~timer() {
        [[maybe_unused]] auto const result = close();
    };

    [[nodiscard]] bool valid() const noexcept {
        std::shared_lock lock(mutex_);
        return descriptor_ >= 0;
    }

    template <typename Period>
    [[nodiscard]] int32_t arm(duration<Period>&& interval) const noexcept {
        return arm(std::chrono::duration_cast<std::chrono::nanoseconds>(interval).count());
    }

    template <typename Period, typename Function, typename... Args>
    void arm(duration<Period>&& interval,
             std::promise<int32_t>&& promise,
             Function&& func, Args&&... args) const noexcept {
        if (descriptor_ == -1) {
            promise.set_value(-1);
        }
        else {
            auto result = arm(std::forward<duration<Period>>(interval));

            if (result == 0) {
                using tuple = std::tuple<std::decay_t<Function>, std::decay_t<Args>...>;
                auto decay_copied = std::make_unique<tuple>(std::forward<Function>(func), std::forward<Args>(args)...);

                std::thread([this](std::promise<int32_t>&& p, decltype(decay_copied)&& params) {
                    int32_t result = 0;
                    while (!disarmed()) {
                        result = wait();
                        if (result == 0) {
                            invoke<tuple>(tuple(*params), std::make_index_sequence<1 + sizeof...(Args)>{});
                        }
                        else {
                            break;
                        }
                    }

                    p.set_value(result);
                }, std::forward<std::promise<int32_t>>(promise), std::move(decay_copied)).detach();
            }
            else {
                promise.set_value(result);
            }
        }
    }

    [[nodiscard]] int32_t disarm() const noexcept;
    [[nodiscard]] bool disarmed() const noexcept;
    [[nodiscard]] int32_t wait() const noexcept;

protected:
    [[nodiscard]] int32_t open() noexcept;
    [[nodiscard]] int32_t close() noexcept;

    [[nodiscard]] int32_t arm(int64_t nanoseconds) const noexcept;

    using timer_interval = std::tuple<int32_t, int32_t, int32_t, int32_t>;
    [[nodiscard]] virtual int32_t timer_spec(int64_t nanoseconds, timer_interval &interval) const noexcept;
};

class deadline : public timer {
public:
    deadline() = default;

    explicit deadline(timer_clock_t clock_type) noexcept
    : timer(clock_type) {
    }

    deadline(const deadline&) = delete;
    deadline(deadline&& other) = default;

    deadline& operator=(const deadline&) = delete;
    deadline& operator=(deadline&& other) = default;

    ~deadline() override = default;

    [[nodiscard]] int32_t timer_spec(int64_t nanoseconds, timer_interval &interval) const noexcept override;
};
}
#endif //RSABO_TIMER_H
