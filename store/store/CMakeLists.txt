cmake_minimum_required(VERSION 3.20)
project(store)

set(CMAKE_CXX_STANDARD 20)

include_directories(deps/spdlog-1.14.1/include)
add_subdirectory(deps/spdlog-1.14.1)

aux_source_directory(. SOURCES)
add_executable(test_store test_store.cpp mman.c)
add_executable(test_atomic_store test_atomic_store.cpp mman.c)