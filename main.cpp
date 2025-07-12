#include <csignal>
#include <ctime>
#include <cstring>

/*
int timer_create(clockid_t clockid,
                 struct sigevent *_Nullable restrict sevp,
                 timer_t *restrict timerid);
*/
#include <future>
#include <thread>
#include <chrono>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <unistd.h>

#define _USING_SOCKET

namespace {
#if 0
    class timer {
        int32_t init() noexcept {
            //auto result = ::timer_create(CLOCK_MONOTONIC, )
            return -1;
        }
    };
#endif

    struct timer_signal_data {
        timer_t timer_id_;
        int socket_;
    };

    constexpr std::string_view notify_address{"/tmp/timer_notify"};

    void timer_thread_handler(sigval value) {
        std::cout << "Timer elapsed!\n";
        // Do something with the si pointer
        auto *tsd = static_cast<timer_signal_data*>(value.sival_ptr);
        constexpr auto eof = static_cast<unsigned char>(0xff);
#ifdef _USING_SOCKET
        [[maybe_unused]] auto result = ::write(tsd->socket_, &eof, 1);
#endif
    }

    void timer_signal_handler(int sig, siginfo_t *si, void *uc) {
        std::cout << "Signal received!\n";
        // Do something with the si pointer
        auto *tsd = static_cast<timer_signal_data*>(si->si_value.sival_ptr);
        constexpr auto eof = static_cast<unsigned char>(0xff);
#ifdef _USING_SOCKET
        [[maybe_unused]] auto result = ::write(tsd->socket_, &eof, 1);
#endif
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

    timer_signal_data tsd {
        .timer_id_ = nullptr,
        .socket_ = -1
    };

#ifdef _USING_SOCKET
    tsd.socket_ = ::socket(PF_UNIX, SOCK_STREAM, IPPROTO_IP);
    if (tsd.socket_ == -1) {
        std::cerr << "socket() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    struct sockaddr_un sock_address{.sun_family = PF_UNIX};
    ::strncpy(sock_address.sun_path, notify_address.data(), notify_address.length());

    auto listening_socket = ::socket(PF_UNIX, SOCK_STREAM, IPPROTO_IP);
    if (listening_socket == -1) {
        std::cerr << "listening socket() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    if (::bind(listening_socket,
            static_cast<const sockaddr*>(static_cast<const void *>(&sock_address)),
        sizeof(sock_address)) == -1) {
        std::cerr << "bind() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    if (::listen(listening_socket, 1) == -1) {
        std::cerr << "listen() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Starting listener thread!\n";

    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    auto listener = std::thread([](int sock, std::promise<void>&& p) {
        auto read_sock = ::accept(sock, nullptr, nullptr);
        if (read_sock == -1) {
            std::cerr << "accept() failed with error " << errno << std::endl;
        }
        else {
            std::cout << "Connection accepted!\n";
            unsigned char buffer[256];
            auto const read_result = ::read(read_sock, buffer, sizeof buffer);
            if (read_result == -1) {
                std::cerr << "read() failed with error " << errno << std::endl;
            }
            else {
                std::cout << "Received " << read_result << " bytes!" << std::endl;
            }

            if (::shutdown(read_sock, SHUT_RD)) {
                std::cerr << "shutdown() failed with error " << errno << std::endl;
            }

            ::close(read_sock);
        }

        p.set_value();
    }, listening_socket, std::move(promise));

    std::this_thread::yield();

    if (::connect(tsd.socket_,
            static_cast<const sockaddr*>(static_cast<const void *>(&sock_address)),
            sizeof(sock_address)) == -1) {
        std::cerr << "connect() failed with error " << errno << std::endl;
    }

    std::cout << "Connection established!\n";
#endif
    struct sigevent sev {
        .sigev_value = { .sival_ptr = &tsd },
        .sigev_signo = timer_signal,
#if 0
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = timer_thread_handler,
        .sigev_notify_attributes = nullptr
#else
        .sigev_notify = SIGEV_SIGNAL
#endif
    };

    if (::timer_create(CLOCK_MONOTONIC, &sev, &tsd.timer_id_) == -1) {
        std::cerr << "timer_create() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Timer successfully created!\n";

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

    if (::timer_settime(tsd.timer_id_, 0, &its, nullptr) == -1) {
        std::cerr << "timer_settime() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Timer started!\n";

    if (::sigprocmask(SIG_UNBLOCK, &mask, nullptr) == -1) {
        std::cerr << "sigprocmask() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    //std::this_thread::sleep_for(std::chrono::seconds(15));

#ifdef _USING_SOCKET
    unsigned int counter = 0;
    while (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
        std::cout << ++counter << '\n';
    }
#endif

#ifndef _USING_SOCKET
    if (::sigprocmask(SIG_UNBLOCK, &mask, nullptr) == -1) {
        std::cerr << "sigprocmask() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }
#endif

    if (::timer_delete(tsd.timer_id_) == -1) {
        std::cerr << "timer_delete() failed with error " << errno << std::endl;
        return EXIT_FAILURE;
    }

    ::close(tsd.socket_);
#ifdef _USING_SOCKET
    // Wait for the timer to elapse
    listener.join();

    ::close(listening_socket);

    ::unlink((notify_address.data()));
#endif

    return EXIT_SUCCESS;
}