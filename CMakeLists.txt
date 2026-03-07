cmake_minimum_required(VERSION 3.15)
project(lunalink_core LANGUAGES CXX)

# Options
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(BUILD_TESTING "Build unit tests" ON)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)

# --- C++ Standard & Flags ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warning Flags (NASA/ESA/JAXA Compliance)
if(MSVC)
    set(LUNALINK_WARNING_FLAGS /W4 /WX /permissive- /GS /volatile:iso)
else()
    set(LUNALINK_WARNING_FLAGS
        -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion
        -Wshadow
        -Wdouble-promotion
        -Wnull-dereference
        -Wundef
        -Wcast-align
        -Wformat=2
        -Wold-style-cast
        -Wimplicit-fallthrough
        -Woverloaded-virtual
        # Determinism
        -fno-fast-math
        -ffp-contract=off
        "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
        # Hardening
        -fstack-protector-strong
        -fno-common
        -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
        )
    # Linker Hardening (NASA/ESA)
    if(NOT APPLE)
        set(LUNALINK_LINKER_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack")
    endif()

    # Mission-critical safety flags (Core only)
    set(LUNALINK_CORE_FLAGS -fno-exceptions -fno-rtti)
endif()

# --- Core Library ---
add_library(lunalink_core
    cpp/signal/prn_table.cpp
    cpp/signal/prn_table_weil10230.cpp
    cpp/signal/prn_table_weil1500.cpp
    cpp/signal/prn.cpp
    cpp/signal/modulator.cpp
    cpp/signal/matched_code.cpp
    cpp/signal/iq_mux.cpp
    cpp/signal/bch.cpp
    cpp/signal/bch_table.cpp
    cpp/signal/frame.cpp
    cpp/signal/ldpc.cpp
    cpp/signal/ldpc_tables.cpp
)
target_include_directories(lunalink_core PUBLIC cpp/include)
set_target_properties(lunalink_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(lunalink_core PRIVATE ${LUNALINK_WARNING_FLAGS} ${LUNALINK_CORE_FLAGS})
target_link_options(lunalink_core PRIVATE ${LUNALINK_LINKER_FLAGS})

# --- Python Bindings ---
if(BUILD_PYTHON_BINDINGS)
    find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)

    python_add_library(_afs MODULE cpp/bindings/afs_module.cpp)
    target_link_libraries(_afs PRIVATE lunalink_core pybind11::module)
    # Bindings get standard warnings but NOT -fno-rtti (needed by pybind11)
    target_compile_options(_afs PRIVATE ${LUNALINK_WARNING_FLAGS})
    set_target_properties(_afs PROPERTIES CXX_VISIBILITY_PRESET "hidden")

    # Install the extension into the package structure
    install(TARGETS _afs DESTINATION lunalink/afs)
endif()

# --- Unit Tests ---
if(BUILD_TESTING)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
          Catch2
          GIT_REPOSITORY https://github.com/catchorg/Catch2.git
          GIT_TAG        f90e8f50a6e76e3492ee9aadfe1014677c4807d2 # v3.4.0 (Pinned SHA for Supply Chain Security)
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    enable_testing()

    add_executable(test_lunalink
        cpp/tests/test_prn.cpp
        cpp/tests/test_modulator.cpp
        cpp/tests/test_matched_code.cpp
        cpp/tests/test_iq_mux.cpp
        cpp/tests/test_bch.cpp
        cpp/tests/test_frame.cpp
        cpp/tests/test_coverage_gap.cpp
        cpp/tests/test_safety.cpp
        cpp/tests/test_ldpc.cpp
        cpp/tests/test_safety_extended.cpp
    )
    target_link_libraries(test_lunalink PRIVATE lunalink_core Catch2::Catch2WithMain)
    target_compile_options(test_lunalink PRIVATE ${LUNALINK_WARNING_FLAGS})

    include(Catch OPTIONAL RESULT_VARIABLE CATCH_MODULE_FOUND)
    if(CATCH_MODULE_FOUND)
        catch_discover_tests(test_lunalink)
    else()
        message(WARNING "Catch2 CMake module not found. CTest integration disabled.")
    endif()
endif()

# --- Coverage Analysis ---
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lunalink_core PRIVATE --coverage)
    target_link_options(lunalink_core PRIVATE --coverage)
    
    target_compile_options(test_lunalink PRIVATE --coverage)
    target_link_options(test_lunalink PRIVATE --coverage)
endif()

# --- Sanitizers ---
if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(lunalink_core PRIVATE ${SANITIZE_FLAGS})
    target_link_options(lunalink_core PRIVATE ${SANITIZE_FLAGS})
    
    target_compile_options(test_lunalink PRIVATE ${SANITIZE_FLAGS})
    target_link_options(test_lunalink PRIVATE ${SANITIZE_FLAGS})
endif()
