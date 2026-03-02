cmake_minimum_required(VERSION 3.15)
project(lsis_afs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(BUILD_TESTING "Build C++ unit tests" OFF)

# Core C++ library — no Python dependency, linked by both bindings and tests.
add_library(lsis_afs_core STATIC cpp/example_core.cpp)
target_include_directories(lsis_afs_core PUBLIC cpp/include)
target_compile_options(lsis_afs_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Python extension
if(BUILD_PYTHON_BINDINGS)
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
    find_package(pybind11 CONFIG REQUIRED)

    pybind11_add_module(_example cpp/example.cpp)
    target_link_libraries(_example PRIVATE lsis_afs_core)
    install(TARGETS _example DESTINATION lsis_afs)
endif()

# C++ unit tests
if(BUILD_TESTING)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.6.0
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(test_example cpp/tests/test_example.cpp)
    target_link_libraries(test_example PRIVATE lsis_afs_core Catch2::Catch2WithMain)
    target_compile_options(test_example PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(test_example)
endif()
