#include "../timer.h"

#include <iostream>

auto main(int argc, char** argv) -> int {
    using namespace std::chrono_literals;

    rsabo::timer t;
    auto result = t.arm(1s);

    if (result) {
        rsabo::report_error(std::cerr, result);
    }

    for (auto i = 0; i < 2; ++i) {
        result = t.wait();

        if (result) {
            rsabo::report_error(std::cerr, result);
        }
    }

    result = t.disarm();

    if (result) {
        rsabo::report_error(std::cerr, result);
    }

    rsabo::timer t1(2s, []() {
        static int32_t counter = 0;
        std::cout << ++counter << std::endl;
    });

    if (t.error()) {
        rsabo::report_error(std::cerr, t.error());
    }

    std::this_thread::sleep_for(15s);

    return EXIT_SUCCESS;
}