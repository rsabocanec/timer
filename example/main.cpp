#include "../timer.h"

#include <iostream>

auto main(int argc, char** argv) -> int {
    using namespace std::chrono_literals;

    {
    std::cout << "Timer example #1\n";

    rsabo::timer t;
    auto result = t.arm(1s);

    if (result) {
        rsabo::report_error(std::cerr, result);
    }

    for (auto i = 0; i < 10; ++i) {
        result = t.wait();

        if (result) {
            rsabo::report_error(std::cerr, result);
        }
        else {
            static int32_t counter = 0;
            std::cout << ++counter << std::endl;
        }
    }

    result = t.disarm();

    if (result) {
        rsabo::report_error(std::cerr, result);
    }
    }

    {
    std::cout << "Timer example #2\n";
    rsabo::timer t;

    std::promise<int32_t> promise{};
    std::future<int32_t> future = promise.get_future();

    t.arm(1s, std::move(promise), []() {
        static int32_t counter = 0;
        std::cout << ++counter << std::endl;
    });

    std::this_thread::sleep_for(14500ms);

    auto result = t.disarm();

    if (result) {
        rsabo::report_error(std::cerr, result);
    }

    result = future.get();

    if (result) {
        rsabo::report_error(std::cerr, result);
    }
    }

    return EXIT_SUCCESS;
}