cmake_minimum_required(VERSION 3.15)
project(lunalink_core LANGUAGES CXX)

# Options
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(BUILD_TESTING "Build unit tests" ON)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(LUNALINK_FAULT_INJECTION "Enable fault-injection APIs for SWIFI tests" OFF)
option(ENABLE_LLVM_COVERAGE "Enable LLVM source-based coverage (MC/DC, requires Clang)" OFF)
option(ENABLE_STACK_USAGE "Enable -fstack-usage to produce .su files for stack analysis" OFF)

# --- SCI Header Generation ---
# Inject git SHA, build timestamp (seconds since J2000), and pre-computed CRC-32
# into a generated header consumed by sci_header.cpp.
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE LUNALINK_GIT_SHA_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT LUNALINK_GIT_SHA_RAW OR LUNALINK_GIT_SHA_RAW STREQUAL "")
    set(LUNALINK_GIT_SHA_RAW "0000000000000000000000000000000000000000")
endif()

execute_process(
    COMMAND git describe --tags --match "v*" --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE LUNALINK_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT LUNALINK_VERSION OR LUNALINK_VERSION STREQUAL "")
    set(LUNALINK_VERSION "v0.0.0-unknown")
endif()

string(TIMESTAMP LUNALINK_BUILD_TIMESTAMP_UNIX "%s")
math(EXPR LUNALINK_BUILD_TIMESTAMP_J2000 "${LUNALINK_BUILD_TIMESTAMP_UNIX} - 946728000")

# Compute SCI header CRC-32 over (git_sha[41] + lsis_version[16] + version[24] + build_timestamp[4]).
# Note: build_timestamp is serialised in *native* byte order to match the
# in-memory layout that sci_header_verify() CRCs at runtime. We use
# sys.byteorder to pick '<I' (LE) or '>I' (BE) so the CRC is consistent
# across architectures. (Fixes: CRC mismatch on big-endian targets.)
execute_process(
    COMMAND python3 -c
        "import struct,sys; bo='<' if sys.byteorder=='little' else '>'; v=b'${LUNALINK_VERSION}'[:23].ljust(24,b'\\x00'); d=b'${LUNALINK_GIT_SHA_RAW}'[:40]+b'\\x00'+b'LSIS-V1.0-2025\\x00\\x00'+v+struct.pack(bo+'I',${LUNALINK_BUILD_TIMESTAMP_J2000}); c=0xFFFFFFFF;\nexec('for x in d:\\n  c^=x\\n  for _ in range(8):\\n    c=(c>>1)^(0xEDB88320 if c&1 else 0)'); print(hex(~c&0xFFFFFFFF))"
    OUTPUT_VARIABLE LUNALINK_SCI_CRC32_HEX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT LUNALINK_SCI_CRC32_HEX OR LUNALINK_SCI_CRC32_HEX STREQUAL "")
    set(LUNALINK_SCI_CRC32_HEX "0x00000000")
endif()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/sci_header_generated.hpp.in
    ${CMAKE_BINARY_DIR}/generated/sci_header_generated.hpp
    @ONLY
)

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
    # codes/ — Spreading code generation (Gateway 1)
    cpp/codes/prn_table.cpp
    cpp/codes/prn_table_weil10230.cpp
    cpp/codes/prn_table_weil1500.cpp
    cpp/codes/prn.cpp
    cpp/codes/matched_code.cpp
    cpp/codes/gold.cpp
    cpp/codes/legendre.cpp
    cpp/codes/weil.cpp
    cpp/codes/weil_parameters.cpp
    # encoding/ — Forward Error Correction (Gateway 2)
    cpp/encoding/bch.cpp
    cpp/encoding/bch_table.cpp
    cpp/encoding/ldpc.cpp
    cpp/encoding/ldpc_tables.cpp
    cpp/encoding/crc.cpp
    cpp/encoding/interleaver.cpp
    # messages/ — Framing (Gateway 3)
    cpp/messages/frame.cpp
    cpp/messages/frame_decode.cpp
    # signal/ — Baseband & RX (Gateway 4-5)
    cpp/signal/modulator.cpp
    cpp/signal/iq_mux.cpp
    cpp/signal/correlator.cpp
    cpp/signal/sync_detector.cpp
    # platform/ — System infrastructure
    cpp/platform/sci_header.cpp
    cpp/platform/post.cpp
    cpp/platform/signal_chain_fsm.cpp
    cpp/platform/watchdog.cpp
)
target_include_directories(lunalink_core PUBLIC cpp/include)
target_include_directories(lunalink_core PRIVATE ${CMAKE_BINARY_DIR}/generated)
set_target_properties(lunalink_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(lunalink_core PRIVATE ${LUNALINK_WARNING_FLAGS} ${LUNALINK_CORE_FLAGS})
target_link_options(lunalink_core PRIVATE ${LUNALINK_LINKER_FLAGS})

# Fault injection APIs are enabled either by the explicit CMake option or
# automatically when testing is enabled. This ensures lunalink_core and
# lunalink_swifi_tests share the same struct layout (no ABI mismatch).
if(LUNALINK_FAULT_INJECTION OR BUILD_TESTING)
    target_compile_definitions(lunalink_core PUBLIC LUNALINK_FAULT_INJECTION=1)
endif()

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
        # codes/
        cpp/tests/codes/test_prn.cpp
        cpp/tests/codes/test_prn_generators.cpp
        cpp/tests/codes/test_matched_code.cpp
        # encoding/
        cpp/tests/encoding/test_bch.cpp
        cpp/tests/encoding/test_ldpc.cpp
        cpp/tests/encoding/test_crc.cpp
        cpp/tests/encoding/test_interleaver.cpp
        # messages/
        cpp/tests/messages/test_frame.cpp
        # signal/
        cpp/tests/signal/test_modulator.cpp
        cpp/tests/signal/test_iq_mux.cpp
        cpp/tests/signal/test_correlator.cpp
        cpp/tests/signal/test_sync_detector.cpp
        # platform/
        cpp/tests/platform/test_safety.cpp
        # integration/
        cpp/tests/integration/test_e2e.cpp
        # tool_validation/
        cpp/tests/tool_validation/test_coverage_canary.cpp
        # performance/
        cpp/tests/performance/test_bch_ber.cpp
        cpp/tests/performance/test_ldpc_ber.cpp
        cpp/tests/performance/test_correlator_perf.cpp
        cpp/tests/performance/test_sync_detection_rate.cpp
        cpp/tests/performance/test_wcet.cpp
    )
    target_link_libraries(test_lunalink PRIVATE lunalink_core Catch2::Catch2WithMain)
    target_include_directories(test_lunalink PRIVATE cpp/tests)
    target_compile_options(test_lunalink PRIVATE ${LUNALINK_WARNING_FLAGS})

    include(Catch OPTIONAL RESULT_VARIABLE CATCH_MODULE_FOUND)
    if(CATCH_MODULE_FOUND)
        # Exclude slow test suites from default CTest runs.
        # Run explicitly: task wcet / task ber / task performance / task integration
        catch_discover_tests(test_lunalink TEST_SPEC "~[wcet]~[channel]~[e2e]")
    else()
        message(WARNING "Catch2 CMake module not found. CTest integration disabled.")
    endif()

    # --- SWIFI (fault injection) test suite ---
    add_executable(lunalink_swifi_tests
        cpp/tests/platform/test_swifi_platform.cpp
        cpp/tests/encoding/test_swifi_encoding.cpp
        cpp/tests/signal/test_swifi_signal.cpp
        cpp/tests/messages/test_swifi_messages.cpp
        cpp/tests/codes/test_swifi_codes.cpp
    )
    target_link_libraries(lunalink_swifi_tests PRIVATE lunalink_core Catch2::Catch2WithMain)
    # -Wno-c2y-extensions: Catch2 TEST_CASE macro uses __COUNTER__ which newer
    # LLVM clang warns about as a C2y extension; suppress to keep -Werror clean.
    target_compile_options(lunalink_swifi_tests PRIVATE ${LUNALINK_WARNING_FLAGS}
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-c2y-extensions>)
    target_compile_definitions(lunalink_swifi_tests PRIVATE LUNALINK_FAULT_INJECTION=1)
    target_include_directories(lunalink_swifi_tests PRIVATE ${CMAKE_BINARY_DIR}/generated)

    if(CATCH_MODULE_FOUND)
        catch_discover_tests(lunalink_swifi_tests
            TEST_SPEC ""
            TEST_PREFIX "swifi."
            PROPERTIES LABELS swifi
        )
    endif()
endif()

# --- Coverage Analysis ---
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lunalink_core PRIVATE --coverage)
    target_link_options(lunalink_core PRIVATE --coverage)
    
    target_compile_options(test_lunalink PRIVATE --coverage)
    target_link_options(test_lunalink PRIVATE --coverage)
endif()

# --- LLVM Source-Based Coverage (MC/DC) ---
if(ENABLE_LLVM_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # -Wno-unknown-warning-option: suppress errors from version-specific warnings (e.g.
    #   c2y-extensions in Clang 22, pgo-coverage in Clang 20+) when building on older LLVM.
    # -Wno-error=c2y-extensions: Allow __COUNTER__ in Catch2 without erroring on Clang 22.
    # -Wno-error=pgo-coverage: Allow skipping MC/DC for unsupported nested boolean expressions.
    set(LLVM_COV_FLAGS -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc -Wno-unknown-warning-option -Wno-error=c2y-extensions -Wno-error=pgo-coverage)
    target_compile_options(lunalink_core PRIVATE ${LLVM_COV_FLAGS})
    target_link_options(lunalink_core PRIVATE ${LLVM_COV_FLAGS})
    if(BUILD_TESTING)
        target_compile_options(test_lunalink PRIVATE ${LLVM_COV_FLAGS})
        target_link_options(test_lunalink PRIVATE ${LLVM_COV_FLAGS})
        target_compile_options(lunalink_swifi_tests PRIVATE ${LLVM_COV_FLAGS})
        target_link_options(lunalink_swifi_tests PRIVATE ${LLVM_COV_FLAGS})
    endif()
elseif(ENABLE_LLVM_COVERAGE AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "ENABLE_LLVM_COVERAGE requires Clang. Use CC=clang CXX=clang++.")
endif()

# --- Sanitizers ---
if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(lunalink_core PRIVATE ${SANITIZE_FLAGS})
    target_link_options(lunalink_core PRIVATE ${SANITIZE_FLAGS})
    
    target_compile_options(test_lunalink PRIVATE ${SANITIZE_FLAGS})
    target_link_options(test_lunalink PRIVATE ${SANITIZE_FLAGS})

    target_compile_options(lunalink_swifi_tests PRIVATE ${SANITIZE_FLAGS})
    target_link_options(lunalink_swifi_tests PRIVATE ${SANITIZE_FLAGS})
endif()

# --- Stack Usage Analysis ---
if(ENABLE_STACK_USAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lunalink_core PRIVATE -fstack-usage)
endif()

# [[private-begin]]
# --- Proprietary Subsystems (not included in OSS export) ---
# add_subdirectory(cpp/inference)
# add_subdirectory(cpp/integrity)
# add_subdirectory(cpp/hal)
# add_subdirectory(cpp/fpga)
# [[private-end]]
