cmake_minimum_required(VERSION 3.20)
project(metropolis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MET_BUILD_TESTS "Build unit tests" ON)
option(MET_BUILD_EXAMPLES "Build examples" OFF)

if(MET_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(platform)

add_subdirectory(color-engine)
add_subdirectory(vfx-engine)
add_subdirectory(motion)
