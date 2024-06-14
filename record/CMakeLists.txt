cmake_minimum_required(VERSION 3.22)
project(dirruptor)
set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_STANDARD 23)

include_directories(deps/spdlog-1.14.1/include)
add_subdirectory(deps/spdlog-1.14.1)

add_executable(spmc test.cpp)
target_link_libraries(spmc PUBLIC spdlog::spdlog_header_only)
