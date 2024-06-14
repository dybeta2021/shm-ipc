cmake_minimum_required(VERSION 3.22)
project(record)
set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_STANDARD 23)

include_directories(deps/spdlog-1.14.1/include)
add_subdirectory(deps/spdlog-1.14.1)

aux_source_directory(. SRC)
add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog_header_only)
