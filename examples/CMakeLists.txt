cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(examples)

# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(.. bpromise/)

add_executable(tcp_echo_server tcp_echo_server.cpp)

target_link_libraries(tcp_echo_server bpromise)
