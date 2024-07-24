
cmake_minimum_required(VERSION 3.20)
project(storepy)

set(CMAKE_CXX_STANDARD 20)
set(PYTHON_EXECUTABLE /Users/dy/Software/miniconda3/bin/python)

include_directories(deps/spdlog-1.14.1/include)
add_subdirectory(deps/spdlog-1.14.1)
include_directories(deps/pybind11-2.13.1/include)
add_subdirectory(deps/pybind11-2.13.1)

aux_source_directory(. SOURCE)
pybind11_add_module(${PROJECT_NAME} SHARED ${SOURCE})

#set(THREADS_PREFER_PTHREAD_FLAG ON)
#find_package(Threads REQUIRED)
#target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)