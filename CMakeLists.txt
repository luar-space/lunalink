cmake_minimum_required(VERSION 3.15)
project(lunalink LANGUAGES CXX)

if(MSVC)
    message(FATAL_ERROR "MSVC is not supported for space flight determinism.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(BUILD_TESTING "Build C++ unit tests" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_COVERAGE "Enable gcov code coverage instrumentation" OFF)

# Sanitizers — applied globally so every target is instrumented.
if(ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "Sanitizers not supported for ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# Coverage — applied globally so gcov instruments every compiled unit.
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage not supported for ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# Safety-focused warning set used on all first-party targets.
# Warnings are treated as errors (-Werror) so CI cannot pass with new diagnostics.
set(LUNALINK_WARNING_FLAGS
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wdouble-promotion
        -Wnull-dereference
        -Wundef
        -Wcast-align
        -Wformat=2
        -Wold-style-cast
        -Wimplicit-fallthrough
    >
    $<$<CXX_COMPILER_ID:GNU>:
        -Wlogical-op
        -Wduplicated-cond
        -Wduplicated-branches
    >
)

# Core C++ library — no Python dependency, linked by both bindings and tests.
add_library(lunalink_core STATIC
    cpp/signal/prn_table.cpp
    cpp/signal/prn_table_weil10230.cpp
    cpp/signal/prn_table_weil1500.cpp
    cpp/signal/prn.cpp
    cpp/signal/modulator.cpp
    cpp/signal/tiered_code.cpp
    cpp/signal/iq_mux.cpp
    cpp/signal/bch.cpp
)
target_include_directories(lunalink_core PUBLIC cpp/include)
set_target_properties(lunalink_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(lunalink_core PRIVATE ${LUNALINK_WARNING_FLAGS})
target_compile_options(lunalink_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions -fno-rtti -fno-fast-math>
)

# Python extension
if(BUILD_PYTHON_BINDINGS)
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
    find_package(pybind11 CONFIG REQUIRED)

    pybind11_add_module(_afs cpp/bindings/afs_module.cpp)
    target_link_libraries(_afs PRIVATE lunalink_core)
    # pybind11 headers trigger some of the stricter warnings; use the baseline set.
    target_compile_options(_afs PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Werror>
    )
    install(TARGETS _afs DESTINATION lunalink/afs)
endif()

# C++ unit tests
if(BUILD_TESTING)
    enable_testing()

    # Prefer a locally installed Catch2 package to avoid network fetches.
    find_package(Catch2 3 QUIET CONFIG)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.6.0.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    endif()

    add_executable(test_lunalink
        cpp/tests/test_prn.cpp
        cpp/tests/test_modulator.cpp
        cpp/tests/test_tiered_code.cpp
        cpp/tests/test_iq_mux.cpp
        cpp/tests/test_bch.cpp
    )
    target_link_libraries(test_lunalink PRIVATE lunalink_core Catch2::Catch2WithMain)
    target_compile_options(test_lunalink PRIVATE ${LUNALINK_WARNING_FLAGS})

    include(Catch OPTIONAL RESULT_VARIABLE CATCH_MODULE_FOUND)
    if(CATCH_MODULE_FOUND)
        catch_discover_tests(test_lunalink)
    else()
        add_test(NAME test_lunalink COMMAND test_lunalink)
    endif()
endif()
