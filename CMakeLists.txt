cmake_minimum_required(VERSION 3.28.3)
project(timer)

set(CMAKE_CXX_STANDARD 20)

add_library(timer SHARED timer.cpp)
add_library(timer-static STATIC timer.cpp)

set_target_properties(timer-static PROPERTIES OUTPUT_NAME timer)

add_subdirectory(example)
