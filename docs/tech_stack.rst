Technical Stack & Compliance
============================

LunaLink utilizes a dual-layered technical stack designed to satisfy both the 
high-performance, deterministic requirements of **Class A Flight Software (FSW)** 
and the ease-of-use requirements for a **Software Reference Implementation**.

Core Philosophy
---------------

1.  **C++20 for Flight**: The signal processing core is written in modern, 
    deterministic C++20, adhering to strict MISRA-aligned safety profiles.
2.  **Python for Integration**: The primary user interface and validation 
    suite are built in Python 3.12+, utilizing ``pybind11`` for low-overhead 
    communication with the C++ core.
3.  **Auditability**: Every tool in the stack is chosen to provide empirical 
    evidence of compliance with the LSIS-AFS standard and ESA quality mandates.

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
     - Unit testing; pinned by **Commit SHA** to ensure supply-chain integrity.
   * - **Clang-Tidy**
     - ≥ 14.0
     - Static analysis; enforces HICPP, CERT, and CppCoreGuidelines.
   * - **gcovr**
     - ≥ 7.0
     - Coverage enforcement; mandates ≥ 90% C++ line coverage.
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
   * - **pybind11**
     - ≥ 2.13
     - Type-safe C++/Python bridge; exposes core DSP blocks as NumPy-compatible APIs.
   * - **pytest**
     - ≥ 8.0
     - High-level functional testing and interoperability validation.
   * - **ruff**
     - ≥ 0.9
     - Extreme performance linting and formatting (replaces Flake8, Black, Isort).
   * - **pyright**
     - ≥ 1.1
     - Static type checking for 100% type-safe public interfaces.
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
*   **Compliance**: Standardized layout with all source code in ``src/lunalink/``.

Dependency Management
~~~~~~~~~~~~~~~~~~~~~
*   **Mandate**: Virtual environments; exact version pinning.
*   **Compliance**: ``uv sync`` generates a bit-for-bit reproducible environment via ``uv.lock``.

Documentation
~~~~~~~~~~~~~
*   **Mandate**: Docstrings for all public interfaces (PEP 257); Sphinx-based.
*   **Compliance**: NumPy-style docstrings rendered via **Sphinx** with the **Furo** theme.

Type Safety
~~~~~~~~~~~
*   **Mandate**: Type hints (PEP 484) for public APIs.
*   **Compliance**: ``pyright`` enforces strict typing; ``.pyi`` stubs are provided for C++ modules.

Testing & Quality
~~~~~~~~~~~~~~~~~
*   **Mandate**: White-box unit testing; Cyclomatic Complexity ≤ 10.
*   **Compliance**: 
    *   ``pytest-cov`` ensures > 90% coverage.
    *   ``ruff (C90)`` and ``radon cc`` enforce complexity limits.
    *   ``radon hal`` and ``radon mi`` generate ESA-required quality metrics.

CI/CD Pipeline
--------------

The GitHub Actions workflow (``.github/workflows/ci.yml``) executes the 
following checks on every commit:

1.  **Safety Profile**: ASan/UBSan memory and undefined behavior checks.
2.  **Hardening Audit**: Verifies that ``-fstack-protector-strong`` and RELRO are active.
3.  **Traceability**: Confirms that all DSP code is linked to LSIS requirement IDs.
4.  **Performance**: Benchmarks critical paths against Gateway success criteria.
