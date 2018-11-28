cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(bpromise)

# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_HEADERS
	include/bpromise/future.h
	include/bpromise/sockets.h
	include/bpromise/threadpool.h
	include/bpromise/worker.h
)

set(LIB_SOURCES
	src/sockets.cpp
	src/threadpool.cpp
	src/worker.cpp
)

add_library(bpromise STATIC ${LIB_SOURCES})
target_include_directories(bpromise PUBLIC include/)

if(WIN32)
  target_link_libraries(bpromise wsock32 ws2_32)
endif()

install(TARGETS bpromise DESTINATION lib)
install(FILES ${LIB_HEADERS} DESTINATION include)
