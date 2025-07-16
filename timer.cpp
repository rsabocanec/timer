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

// timer implementation
int32_t timer::arm(int64_t nanoseconds) noexcept {
    error_ = 0;
    if (descriptor_ != -1) {
        struct timespec now{};

        if (::clock_gettime(static_cast<int>(clock_type_), &now) != 0) {
            error_ = errno;
        }
        else {
            const struct itimerspec its = {
                .it_interval = {.tv_sec = nanoseconds / 1'000'000'000, .tv_nsec = nanoseconds % 1'000'000'000},
                .it_value = {.tv_sec = now.tv_sec + nanoseconds / 1'000'000'000, .tv_nsec = now.tv_nsec}
            };

            if (::timerfd_settime(descriptor_, TFD_TIMER_ABSTIME, &its, nullptr) == -1) {
                error_ = errno;
            }
        }
    }

    return error_;
}

int32_t timer::disarm() noexcept {
    error_ = EINVAL;;

    if (descriptor_ != -1) {
        error_ = 0;

        constexpr struct itimerspec its = {};

        if (::timerfd_settime(descriptor_, TFD_TIMER_ABSTIME, &its, nullptr) == -1) {
            error_ = errno;
        }
    }

    return error_;
}

bool timer::disarmed() noexcept {
    error_ = EINVAL;;
    if (descriptor_ != -1) {
        struct itimerspec period{};
        if (::timerfd_gettime(descriptor_, &period) == -1) {
            error_ = errno;
        }
        else {
            error_ = 0;
            if (period.it_value.tv_sec != 0 || period.it_value.tv_nsec != 0) {
                return false;
            }
        }
    }

    return true;
}

int32_t timer::wait() noexcept {
    error_ = EINVAL;
    if (descriptor_ != -1) {
        uint64_t exp{};
        auto const result = ::read(descriptor_, &exp, sizeof(exp));
        if (result == -1) {
            error_ = errno;
        }
        else if (result != sizeof(exp)) {
            error_ = EOVERFLOW;
        }
        else {
            error_ = 0;
        }
    }

    return error_;
}

int32_t timer::open() noexcept {
    descriptor_ = ::timerfd_create(static_cast<int>(clock_type_), 0);
    if (descriptor_ == -1) {
        error_ = errno;
    }

    return error_;
}

int32_t timer::close() noexcept {
    error_ = EINVAL;
    if (descriptor_ != -1) {
        error_ = 0;
        auto const result = ::close(descriptor_);
        if (result == -1) {
            error_ = errno;
        }
    }

    descriptor_ = -1;
    return 0;
}
}