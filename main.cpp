//
// Created by r.sabocanec on 13.7.2025..
//
#include <source_location>
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

        explicit timer(uint32_t seconds, uint32_t nanoseconds = 0ul) noexcept {
            if (open() == 0) {
                [[maybe_unused]] auto const result = arm(seconds, nanoseconds);
            }
        }

        timer(const timer&) = delete;
        timer(timer&&) = default;

        timer& operator=(const timer&) = delete;
        timer& operator=(timer&&) = default;

        ~timer() {
            if (descriptor_ != -1) {
                [[maybe_unused]] auto const result = ::close(descriptor_);
            }

            descriptor_ = -1;
        };

        int32_t arm(uint32_t seconds, uint32_t nanoseconds = 0ul) noexcept {
            if (descriptor_ != -1) {
                struct timespec now{};
                if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
                    return report_error("clock_gettime()");
                }

                const struct itimerspec period = {
                    .it_interval = {.tv_sec = seconds, .tv_nsec = nanoseconds},
                    .it_value = {.tv_sec = now.tv_sec + seconds, .tv_nsec = now.tv_nsec}
                };

                if (::timerfd_settime(descriptor_, TFD_TIMER_ABSTIME, &period, nullptr) == -1) {
                    return report_error("arm(), timerfd_settime()");
                }

                return 0;
            }

            return -1;
        }

        int32_t disarm() noexcept {
            constexpr struct itimerspec period = {};

            if (::timerfd_settime(descriptor_, TFD_TIMER_ABSTIME, &period, nullptr) == -1) {
                return report_error("disarm(), timerfd_settime()");
            }

            return 0;
        }

        int32_t wait() noexcept {
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
                    std::cout << "Timer expired " << exp << std::endl;
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
    timer t(1);
    for (auto i = 0; i < 10; ++i) {
        [[maybe_unused]] auto const result = t.wait();
    }

    return EXIT_SUCCESS;
}