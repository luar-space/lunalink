Build Tiers and Compiler Policy
================================

Purpose
-------
Define the three build tiers, their compiler toolchains, and the assurance
obligations that apply at each tier.  All tiers share the same C++ source
tree (``cpp/``); the differences are in compiler selection, flags, and
verification scope.

Build Tier Overview
-------------------

.. list-table::
   :header-rows: 1
   :widths: 10 20 25 25 20

   * - Tier
     - Purpose
     - Compiler
     - Key Flags
     - Assurance Scope
   * - **1 — Flight**
     - Binary for the target processor (PolarFire SoC / RISC-V)
     - ``arm-rtems-gcc 12.3.1`` (change-controlled)
     - ``-fno-exceptions -fno-rtti -ffp-contract=off -ffile-prefix-map``
     - Full ECSS Cat B: MC/DC, FMEA, SWIFI, deterministic builds, MD5 verification
   * - **2 — CI / Digital Twin**
     - Verification evidence on host
     - Clang 19 (CI), GCC (cross-arch)
     - Same as Tier 1 + sanitizers (ASan, UBSan)
     - Same tests, coverage, SWIFI; no deterministic-binary mandate
   * - **3 — Python Library**
     - ``pip install lunalink`` for researchers
     - Platform-native: GCC (manylinux), Clang (macOS), MSVC (Windows)
     - Best-effort hardening; see below
     - pytest passes, wheel installs, no certification claims

Tier 1 — Flight
----------------
Flight code includes:

- ``cpp/{codes,encoding,messages,signal,platform}/`` implementation sources
- ``cpp/include/lunalink/`` public headers

Flight build requirements:

1. Must compile with ``-fno-exceptions`` and ``-fno-rtti``.
2. Forbidden optimisation flags (``-ffast-math``, ``-funroll-loops``,
   ``-fomit-frame-pointer``, ``-fstrict-aliasing``) must not appear.
3. Must enable hardening flags (stack protection, FP stability).
4. Must produce deterministic binaries for identical inputs
   (``-ffile-prefix-map``, ``-ffp-contract=off``).
5. Compiler upgrades require formal change control: full MC/DC re-run,
   assembly comparison, and reproducible-build MD5 re-certification.

Tier 2 — CI / Digital Twin
---------------------------
Uses the same source and flags as Tier 1, plus sanitizers and extended
test instrumentation.  Runs on x86_64, ARM64 (LE), and s390x (BE) to
verify endianness correctness and cross-platform portability.

Non-flight code (tests, tools, benchmarks, bindings) may enable broader
C++ features (exceptions, RTTI) when explicitly configured, but must
never be linked into flight binaries.

Tier 3 — Python Library
------------------------
The Python extension module (``_afs``) wraps the same ``lunalink_core``
library via nanobind.  Tier 3 builds use the platform-native compiler:

- **Linux**: GCC (manylinux container)
- **macOS**: Apple Clang
- **Windows**: MSVC

Tier 3 compiler flags apply best-effort hardening equivalents:

.. list-table::
   :header-rows: 1
   :widths: 40 30 30

   * - Tier 1/2 (GCC/Clang)
     - Tier 3 (MSVC)
     - Notes
   * - ``-fno-exceptions``
     - ``/EHs-c-``
     - Core library only; nanobind binding re-enables exceptions
   * - ``-fno-rtti``
     - ``/GR-``
     - Core library only
   * - ``-fno-fast-math -ffp-contract=off``
     - ``/fp:strict``
     - Close equivalent; not bit-identical across compilers
   * - ``-fstack-protector-strong``
     - ``/GS``
     - Partial equivalent
   * - ``-ffile-prefix-map``
     - *(no equivalent)*
     - Deterministic builds are a Tier 1 concern only
   * - ``[[gnu::section(...)]]``
     - no-op
     - LUT section pinning is a linker-script / flight concern

**Tier 3 does not make certification claims.**  It exists so researchers
and engineers can evaluate AFS signal processing on any platform without
building from source.  Correctness is validated by the pytest suite, not
by the assurance framework.

Compiler Abstraction
--------------------
Platform-specific constructs are abstracted in
``cpp/include/lunalink/core/safety.hpp``:

- ``LUNALINK_LUT_SECTION`` — ``[[gnu::section(...)]]`` on GCC/Clang, no-op
  on MSVC.
- ``LUNALINK_PACKED_BEGIN`` / ``LUNALINK_PACKED_END`` / ``LUNALINK_PACKED_ATTR``
  — ``[[gnu::packed]]`` on GCC/Clang, ``#pragma pack`` on MSVC.
- ``has_force_set_v<T, U>`` — SFINAE trait replacing a ``requires``
  expression that older MSVC versions do not support.

These macros ensure the flight build (Tier 1) is unchanged; only the
MSVC code path is affected.

Dev/Bench-Only Compile Flags
-----------------------------
A small number of CMake options gate code paths that are useful for
development or for performance benchmarking on the host but **must not**
appear in any Tier 1 (flight) or Tier 2 (CI / digital twin) build.  They
typically add heap allocation, atomic counters, or instrumentation that
would invalidate the assurance evidence accumulated against the flight
configuration.

.. list-table::
   :header-rows: 1
   :widths: 25 15 30 30

   * - CMake option
     - Define
     - What it adds
     - Why forbidden in flight
   * - ``LUNALINK_BENCH_COUNT_FFTS``
     - ``LUNALINK_BENCH_COUNT_FFTS=1``
     - Atomic counters in ``signal::bench`` incremented on every forward
       and inverse FFT call from the proprietary correlator; queryable
       from Python for bench scripts.
     - Adds a global ``std::atomic<uint64_t>`` per direction; counter
       increments contend across PRN-parallel acquisition threads,
       distorting timing.  Counters are unbounded and out-of-scope for
       FMEA / SWIFI evidence.
   * - ``LUNALINK_DEV_RX_FFT_CACHE``
     - ``LUNALINK_DEV_RX_FFT_CACHE=1``
     - ``std::unordered_map`` memoising the rx-FFT result inside
       ``Correlator::correlate_primary_complex``, keyed on the 5-tuple
       ``(prim_rx_ptr, doppler_bits, fft_size, sample_offset, n_samp)``
       where ``doppler_bits`` is ``std::bit_cast<uint32_t>(doppler)``
       (the IEEE-754 bit pattern of the input float — not a quantised
       integer; this preserves bit-exact hits for fractional
       search-grid Dopplers).  Used by the LuGRE campaign to amortise
       the rx-FFT across the PRN sweep.
     - Heap allocation in the correlator inner path; bounded by a
       wholesale-flush 2 GiB payload cap (``kRxFftCacheMaxPayloadBytes``)
       rather than per-entry eviction.  The cache lives in a
       heap-backed ``std::unordered_map``; on platforms where the
       allocator is non-deterministic (e.g. macOS jemalloc replacement,
       Linux glibc pthread arena), allocation timing and bucket layout
       vary across runs — this is the documented reason it cannot be
       promoted to flight (Tier 1 demands deterministic memory
       behaviour for WCET and FMEA).

These options default to ``OFF`` in ``CMakeLists.txt`` and are only enabled
by the ``bench-install`` Taskfile recipe via ``CMAKE_ARGS`` for local
benchmarking on the developer host.

Enforcement
-----------
- The build gate must verify flight targets use the required flags and
  avoid forbidden flags.
- The gate must reject usage of ``throw``, ``try``/``catch``,
  ``dynamic_cast``, and ``typeid`` in flight directories.
- The compile commands database (``compile_commands.json``) is the source of
  truth for flag verification.
- ``scripts/check_build_flags.py`` is the single normative gate.  Its
  ``FORBIDDEN_DEFINES`` list mirrors the Dev/Bench-Only catalogue above; a
  flight-tier translation unit found carrying any of those defines fails
  the gate.  The script scans every translation unit under ``cpp/codes/``,
  ``cpp/encoding/``, ``cpp/messages/``, ``cpp/signal/``, ``cpp/platform/``,
  ``cpp/proprietary/codes/``, and ``cpp/proprietary/signal/`` — i.e. both
  the open-source flight library and the proprietary receiver library
  share the same gate.
- **Enforcement loop**: there is no GitHub Actions workflow that runs
  ``check_build_flags.py`` standalone today.  Enforcement is layered as
  (i) ``task ci-checks`` (which calls ``task check-flags``), invoked
  manually before merge; (ii) the ``pre-commit`` and ``pre-push``
  Taskfile recipes wired into git hooks via ``task install-hooks``;
  (iii) operator discipline.  This matches the no-CI-for-bench
  rationale captured in ``docs/shaping/cislunar_nav/fft-acceleration.md``.
- The Tier 3 Windows build is validated by ``test-python.yml`` (Python 3.12
  on ``windows-latest``).
