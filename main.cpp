#include <csignal>
#include <ctime>

/*
int timer_create(clockid_t clockid,
                 struct sigevent *_Nullable restrict sevp,
                 timer_t *restrict timerid);
*/
#include <thread>
#include <chrono>
#include <iostream>

namespace {
#if 0
    class timer {
        int32_t init() noexcept {
            //auto result = ::timer_create(CLOCK_MONOTONIC, )
            return -1;
        }
    };
#endif

    void timer_signal_handler(int sig, siginfo_t *si, void *uc)
    {
        // Do something with the si pointer
        std::cout << "Timer elapsed!\n";
        ::signal(sig, SIG_IGN);
    }
}

int main(int argc, char** argv) {
    struct sigaction sa {
        .sa_flags = SA_SIGINFO
    };

    sa.sa_sigaction = timer_signal_handler;

    ::sigemptyset((&sa.sa_mask));

    auto timer_signal{SIGRTMIN};

    if (::sigaction(timer_signal, &sa, nullptr) == -1) {
        std::cerr << "sigaction() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    // Block the timer signal temporary
    sigset_t mask{};
    ::sigemptyset(&mask);
    ::sigaddset(&mask, timer_signal);
    if (::sigprocmask(SIG_SETMASK, &mask, nullptr) == -1) {
        std::cerr << "sigprocmask() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    timer_t timer_id{};

    struct sigevent sev {
        .sigev_value = { .sival_ptr = &timer_id },
        .sigev_signo = timer_signal,
        .sigev_notify = SIGEV_SIGNAL
    };

    if (::timer_create(CLOCK_MONOTONIC, &sev, &timer_id) == -1) {
        std::cerr << "timer_create() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    struct itimerspec its {
        .it_interval = {
            .tv_sec = 10,
            .tv_nsec = 0
        },
        .it_value = {
            .tv_sec = 10,
            .tv_nsec = 0,
        }
    };

    if (::timer_settime(timer_id, 0, &its, nullptr) == -1) {
        std::cerr << "timer_settime() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    // Wait for the timer to elapse
    std::this_thread::sleep_for(std::chrono::seconds(11));

    if (::sigprocmask(SIG_UNBLOCK, &mask, nullptr) == -1) {
        std::cerr << "sigprocmask() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    if (::timer_delete(timer_id) == -1) {
        std::cerr << "timer_delete() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}