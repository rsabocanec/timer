#include "timer.h"

#include <ostream>

#include <cstdio>
#include <cstring>
#include <ctime>

#include <sys/timerfd.h>
#include <sys/types.h>

#include <err.h>
#include <unistd.h>

namespace rsabo {
void report_error(
    std::ostream& os,
    int32_t error_num,
    std::string_view prefix,
    std::source_location location) noexcept {

    os  << prefix << "Error " << error_num
        << '[' << ::strerror(error_num) << "]\t"
        << "in " << location.file_name()
        << " (" << location.function_name() << "), line "
        << location.line() << '\n';
}

static_assert(
    static_cast<int>(timer_clock_t::clock_realtime) == CLOCK_REALTIME &&
    static_cast<int>(timer_clock_t::clock_monotonic) == CLOCK_MONOTONIC &&
    static_cast<int>(timer_clock_t::clock_boottime) == CLOCK_BOOTTIME &&
    static_cast<int>(timer_clock_t::clock_realtime_alarm) == CLOCK_REALTIME_ALARM &&
    static_cast<int>(timer_clock_t::clock_boottime_alarm) == CLOCK_BOOTTIME_ALARM
);

// timer implementation
int32_t timer::arm(int64_t nanoseconds) noexcept {
    if (descriptor_ == -1) {
        return -1;
    }

    struct timespec now{};

    if (::clock_gettime(static_cast<int>(clock_type_), &now) != 0) {
        return errno;
    }
    else {
        const struct itimerspec its = {
            .it_interval = {.tv_sec = nanoseconds / 1'000'000'000, .tv_nsec = nanoseconds % 1'000'000'000},
            .it_value = {.tv_sec = now.tv_sec + nanoseconds / 1'000'000'000, .tv_nsec = now.tv_nsec}
        };

        if (::timerfd_settime(descriptor_, TFD_TIMER_ABSTIME, &its, nullptr) == -1) {
            return errno;
        }
    }

    return 0;
}

int32_t timer::disarm() noexcept {
    if (descriptor_ == -1) {
        return -1;
    }

    constexpr struct itimerspec its = {
        .it_interval = {.tv_sec = 0, .tv_nsec = 0},
        .it_value = {.tv_sec = 0, .tv_nsec = 0}};

    if (::timerfd_settime(descriptor_, TFD_TIMER_ABSTIME, &its, nullptr) == -1) {
        return errno;
    }

    return 0;
}

bool timer::disarmed() noexcept {
    if (descriptor_ != -1) {
        struct itimerspec period{};
        if (::timerfd_gettime(descriptor_, &period) != -1) {
            if (period.it_value.tv_sec != 0 || period.it_value.tv_nsec != 0) {
                return false;
            }
        }
    }

    return true;
}

int32_t timer::wait() noexcept {
    if (descriptor_ == -1) {
        return -1;
    }

    uint64_t exp{};

    switch (::read(descriptor_, &exp, sizeof(exp))) {
        case -1:
            return errno;
        case sizeof(exp):
            return 0;
        default:
            return EOVERFLOW;
    }
}

int32_t timer::open() noexcept {
    descriptor_ = ::timerfd_create(static_cast<int>(clock_type_), 0);
    return descriptor_ == -1 ? errno : 0;
}

int32_t timer::close() noexcept {
    if (descriptor_ != -1) {
        auto const result = ::close(descriptor_);
        if (result == -1) {
            return errno;
        }
    }

    return 0;
}
}