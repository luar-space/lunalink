Technical Stack & Compliance
============================

LunaLink runs a two-layer stack.  One layer meets the deterministic,
high-performance demands of **Class B Flight Software (FSW)**; the other keeps
a **Software Reference Implementation** easy to pick up.

Core Philosophy
---------------

1.  **C++20 for Flight**: the signal-processing core is deterministic modern
    C++20, held to a MISRA-aligned safety profile.
2.  **Python for Integration**: Python 3.12+ carries the user interface and
    the validation suite, reaching the C++ core through ``nanobind`` at low
    overhead.
3.  **Auditability**: every tool earns its place by producing evidence of
    compliance with the LSIS-AFS standard and ESA quality mandates.

Flight Software Stack (C++)
---------------------------

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Tool
     - Version
     - Mission Role
   * - **C++ Standard**
     - C++20
     - Primary implementation language; enables ``constinit``, ``span``, and ``bit`` utilities.
   * - **CMake**
     - ≥ 3.15
     - Deterministic build system; enforces hardening flags and remapping.
   * - **Catch2**
     - v3.x
     - Unit testing; pinned by **commit SHA** for supply-chain integrity.
   * - **Clang-Tidy**
     - ≥ 14.0
     - Static analysis; enforces HICPP, CERT, and CppCoreGuidelines.
   * - **llvm-cov**
     - LLVM
     - Source-based coverage with MC/DC; CI enforces ≥ 90% region, ≥ 85% line
       and ≥ 30% MC/DC.
   * - **Sanitizers**
     - LLVM/GNU
     - Runtime validation; ASan and UBSan are active in CI to catch memory/UB faults.

Reference Implementation Stack (Python)
---------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Tool
     - Version
     - Role
   * - **uv**
     - ≥ 0.9
     - Unified dependency management and deterministic environment (``uv.lock``).
   * - **nanobind**
     - ≥ 2.0
     - Type-safe C++/Python bridge; exposes core DSP blocks as NumPy-compatible APIs.
   * - **pytest**
     - ≥ 8.0
     - High-level functional testing and interoperability validation.
   * - **ruff**
     - ≥ 0.9
     - Fast linting and formatting; replaces Flake8, Black and isort.
   * - **pyright**
     - ≥ 1.1
     - Static type checking; every public interface is fully typed.
   * - **radon**
     - ≥ 6.0
     - Quality metrics (Cyclomatic Complexity, Halstead, Maintainability Index).

ESA Python Guidelines Compliance
--------------------------------

LunaLink is audited against **PE-RS-ESA-GS-0871** (ESA Python Coding Guidelines).

Code Style & Naming
~~~~~~~~~~~~~~~~~~~
*   **Mandate**: PEP 8 baseline; ``snake_case`` for functions; ``PascalCase`` for classes.
*   **Compliance**: ``ruff`` enforces these via the ``E``, ``W``, and ``N`` rule sets.

Project Structure
~~~~~~~~~~~~~~~~~
*   **Mandate**: ``src`` layout; explicit ``pyproject.toml``.
*   **Compliance**: all source code sits in ``src/lunalink/``.

Dependency Management
~~~~~~~~~~~~~~~~~~~~~
*   **Mandate**: Virtual environments; exact version pinning.
*   **Compliance**: ``uv sync`` generates a bit-for-bit reproducible environment via ``uv.lock``.

Documentation
~~~~~~~~~~~~~
*   **Mandate**: Docstrings for all public interfaces (PEP 257); Sphinx-based.
*   **Compliance**: NumPy-style docstrings rendered via **Sphinx** with the **Read the Docs** theme.

Type Safety
~~~~~~~~~~~
*   **Mandate**: Type hints (PEP 484) for public APIs.
*   **Compliance**: ``pyright`` enforces strict typing; ``.pyi`` stubs cover the C++ modules.

Testing & Quality
~~~~~~~~~~~~~~~~~
*   **Mandate**: White-box unit testing; Cyclomatic Complexity ≤ 10.
*   **Compliance**:
    *   ``pytest-cov`` enforces the 100% line-coverage gate (the build fails below it).
    *   ``ruff (C90)`` and ``radon cc`` enforce complexity limits.
    *   ``radon hal`` and ``radon mi`` generate ESA-required quality metrics.

CI/CD Pipeline
--------------

CI runs as a set of focused GitHub Actions workflows under
``.github/workflows/`` — lint, Python tests (100% coverage gate), C++
tests, ASan/UBSan sanitizers, clang-tidy static analysis, C++
line/branch/MC/DC coverage, fuzzing, and the Sphinx docs build.  The
full workflow table is in :doc:`developing`.
