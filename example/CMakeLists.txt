cmake_minimum_required(VERSION 3.28.3)

set(CMAKE_CXX_STANDARD 20)

add_executable(timer-example main.cpp)
add_dependencies(timer-example timer)

target_link_libraries(timer-example PRIVATE timer)
