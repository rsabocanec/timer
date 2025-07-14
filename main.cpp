//
// Created by r.sabocanec on 13.7.2025..
//
#include <source_location>
#include <functional>
#include <tuple>
#include <chrono>
#include <thread>
#include <future>

#include <iostream>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <sys/timerfd.h>
#include <sys/types.h>

#include <err.h>
#include <unistd.h>

namespace {
    template <typename Period>
    using duration = std::chrono::duration<int64_t, Period>;

    // Helper method for invoking a method in the timer class
    template <class Tuple, size_t... Indices>
    inline void invoke(Tuple&& func_values, std::index_sequence<Indices...>) noexcept{
        std::invoke(std::move(std::get<Indices>(func_values))...);
    }

    int32_t report_error(
        const std::string_view prefix = std::source_location::current().function_name(),
        const int32_t error_num = errno) noexcept {

        std::cerr   << prefix << " returned error " << error_num
                    << '[' << ::strerror(error_num) << ']' << std::endl;

        return error_num;
    }

    class timer {
        int descriptor_{-1};

        public:
        timer() noexcept {
            [[maybe_unused]] auto const result = open();
        }

        template <typename Period>
        explicit timer(duration<Period>&& interval) noexcept {
            if (open() == 0) {
                [[maybe_unused]] auto const result = arm(std::forward<duration<Period>>(interval));
            }
        }

        template <typename Period, typename Function, typename... Args>
        explicit timer(duration<Period>&& interval, Function&& func, Args&&... args) noexcept {
            if (open() == 0) {
                if (arm(std::forward<duration<Period>>(interval)) == 0) {
                    using tuple = std::tuple<std::decay_t<Function>, std::decay_t<Args>...>;
                    auto decay_copied = std::make_unique<tuple>(std::forward<Function>(func), std::forward<Args>(args)...);

                    std::thread([this](decltype(decay_copied)&& params) {
                        while (!disarmed()) {
                            auto const result = wait();
                            if (result == 0) {
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

        timer(const timer&) = delete;

        timer(timer&& other) noexcept {
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
            }

            return *this;
        }

        ~timer() {
            [[maybe_unused]] auto const result = close();
        };

        template <typename Period>
        [[nodiscard]] int32_t arm(duration<Period>&& interval) const noexcept {
            if (descriptor_ != -1) {
                struct timespec now{};
                // The clock ID can be one of the following:
                // CLOCK_REALTIME
                //       A settable system-wide real-time clock.

                // CLOCK_MONOTONIC
                //       A nonsettable monotonically increasing clock that measures
                //       time from some unspecified point in the past that does not
                //       change after system startup.

                // CLOCK_BOOTTIME (Since Linux 3.15)
                //       Like CLOCK_MONOTONIC, this is a monotonically increasing
                //       clock.  However, whereas the CLOCK_MONOTONIC clock does not
                //       measure the time while a system is suspended, the
                //       CLOCK_BOOTTIME clock does include the time during which the
                //       system is suspended.  This is useful for applications that
                //       need to be suspend-aware.  CLOCK_REALTIME is not suitable
                //       for such applications, since that clock is affected by
                //       discontinuous changes to the system clock.

                // CLOCK_REALTIME_ALARM (since Linux 3.11)
                //       This clock is like CLOCK_REALTIME, but will wake the system
                //       if it is suspended.  The caller must have the
                //       CAP_WAKE_ALARM capability in order to set a timer against
                //       this clock.

                // CLOCK_BOOTTIME_ALARM (since Linux 3.11)
                //       This clock is like CLOCK_BOOTTIME, but will wake the system
                //       if it is suspended.  The caller must have the
                //       CAP_WAKE_ALARM capability in order to set a timer against
                //       this clock.

                if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
                    return report_error("clock_gettime()");
                }

                auto const period = std::chrono::duration_cast<std::chrono::nanoseconds>(interval).count();

                const struct itimerspec its = {
                    .it_interval = {.tv_sec = period / 1'000'000'000, .tv_nsec = period % 1'000'000'000},
                    .it_value = {.tv_sec = now.tv_sec + period / 1'000'000'000, .tv_nsec = now.tv_nsec}
                };

                if (::timerfd_settime(descriptor_, TFD_TIMER_ABSTIME, &its, nullptr) == -1) {
                    return report_error("arm(), timerfd_settime()");
                }

                return 0;
            }

            return -1;
        }

        [[nodiscard]] int32_t disarm() const noexcept {
            constexpr struct itimerspec its = {};

            if (::timerfd_settime(descriptor_, TFD_TIMER_ABSTIME, &its, nullptr) == -1) {
                return report_error("disarm(), timerfd_settime()");
            }

            return 0;
        }

        [[nodiscard]] bool disarmed() const noexcept {
            struct itimerspec period{};
            if (::timerfd_gettime(descriptor_, &period) == -1) {
                [[maybe_unused]] auto const result = report_error("disarmed(), timerfd_gettime()");
            }
            else {
                if (period.it_value.tv_sec != 0 || period.it_value.tv_nsec != 0) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] int32_t wait() const noexcept {
            if (descriptor_ != -1) {
                uint64_t exp{};
                auto const result = ::read(descriptor_, &exp, sizeof(exp));
                if (result == -1) {
                    return report_error("wait(), read()");
                }
                else if (result != sizeof(exp)) {
                    std::cerr << "Read timer returned wrong length: " << result << std::endl;
                }
                else {
                    std::cout << "Timer " << descriptor_ << " expired " << exp << std::endl;
                }

                return 0;
            }

            return -1;
        }

    private:
        int32_t open() noexcept {
            auto const result = ::timerfd_create(CLOCK_MONOTONIC, 0);
            if (result == -1) {
                return report_error();
            }

            descriptor_ = result;
            return 0;
        }

        int32_t close() noexcept {
            if (descriptor_ != -1) {
                auto const result = ::close(descriptor_);
                if (result == -1) {
                    return errno;
                }
            }

            descriptor_ = -1;
            return 0;
        }
    };
}

auto main(int argc, char** argv) -> int {
    using namespace std::chrono_literals;

    timer t;
    auto result = t.arm(1s);

    for (auto i = 0; i < 2; ++i) {
        result = t.wait();
    }

    result = t.disarm();

    timer t1(2s, []() {
        static int32_t counter = 0;
        std::cout << ++counter << std::endl;
    });

    std::this_thread::sleep_for(15s);

    return EXIT_SUCCESS;
}