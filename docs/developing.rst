Developer Guide
===============

Everything needed to work on this repository, from first checkout to cutting
a release.

Prerequisites
-------------

Install these before starting:

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Tool
     - Install
     - Purpose
   * - `uv <https://docs.astral.sh/uv/>`_
     - ``curl -LsSf https://astral.sh/uv/install.sh | sh``
     - Python environment and package management
   * - `Task <https://taskfile.dev>`_
     - ``brew install go-task``
     - Task runner (all common commands go through ``task``)
   * - `CMake <https://cmake.org>`_ ≥ 3.15
     - ``brew install cmake``
     - C++ build system
   * - `Ninja <https://ninja-build.org>`_
     - ``brew install ninja``
     - Fast C++ build backend (optional but recommended)
   * - A C++20 compiler
     - Ships with Xcode CLT on macOS (``xcode-select --install``, Apple Clang 15+) or GCC 13+ / Clang 16+
     - Compiles the C++ extension (``constinit``, ``span``)
   * - `clang-tidy <https://clang.llvm.org/extra/clang-tidy/>`_
     - ``brew install llvm``
     - Static analysis for MISRA/HICPP compliance (optional for local dev)
   * - `cppcheck <https://cppcheck.sourceforge.io/>`_
     - ``brew install cppcheck``
     - Secondary safety-critical static analyzer

Getting Started
---------------

Clone the repository and run the one-time setup::

    # from a clone of this repository
    cd lunalink
    task install        # create .venv and install all dependencies
    task install-hooks  # register the pre-commit hook

After this, ``task`` (with no arguments) runs the everyday quality pipeline:
install → lint → test → docs-strict.

Available Tasks
---------------

Run ``task --list`` to see all tasks. The most common ones are:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Command
     - Description
   * - ``task install``
     - Create/update the virtual environment and install dependencies.
   * - ``task build``
     - Force-recompile the C++ extension (use after editing ``cpp/``).
   * - ``task test``
     - Rebuild the C++ extension, run Catch2 tests, then run pytest.
   * - ``task test-cpp``
     - Build and run only the C++ Catch2 tests via CTest (excludes ``[slow]``).
   * - ``task coverage-cpp``
     - Build and run C++ tests with LLVM source-based coverage (region, line,
       branch, MC/DC), then generate an HTML report with ``llvm-cov``.
   * - ``task lint``
     - Run ``pyright``, ``cppcheck``, ``ruff`` (format + lint, with auto-fix)
       and the docstring sync check.
   * - ``task metrics``
     - Report cyclomatic complexity and maintainability index via radon.
   * - ``task docs-strict``
     - Fast warnings-as-errors Sphinx build to ``docs/_build/html/``; the
       docs gate, and the one to run before a commit.
   * - ``task docs-build``
     - The same build, but re-executing the notebooks first.  Use it only
       when notebook outputs need regenerating.
   * - ``task docs-serve``
     - Serve the documentation locally with live reload on port 8000.
   * - ``task install-hooks``
     - Register the git pre-commit hook in ``.githooks/``.
   * - ``task sanitize``
     - Build and run C++ tests with AddressSanitizer + UBSan.
   * - ``task tidy``
     - Run clang-tidy HICPP static analysis on all C++ signal sources.
   * - ``task wcet``
     - Build in Release mode and run the ``[wcet]``-tagged timing benchmarks,
       generating ``wcet_profile.rst``.
   * - ``task pre-commit``
     - What the git hook runs: build, lint-check, clang-tidy on staged files,
       test-cpp, test-py.
   * - ``task ci-checks``
     - Full local CI gate: build, lint, formatting, static analysis, build
       flags, C++ tests, fault injection, traceability, doc metrics, tool
       validation, Python tests, sanitizers, docs.
   * - ``task recertify``
     - Run all recertification gates locally and write
       ``docs/assurance/recertification_record.rst``.
       Override defaults: ``task recertify MILESTONE=PDR REASON="..." AUTHORITY="..."``

Daily Workflow
--------------

**Editing Python code**

No rebuild needed: the package installs in editable mode, so a change under
``src/`` takes effect at once::

    # edit src/lunalink/...
    task test    # runs build (no-op if C++ unchanged), then all tests
    task lint    # type check + format + lint

**Editing C++ code**

Any edit under ``cpp/`` needs a rebuild::

    # edit cpp/...
    task build   # recompiles the extension and reinstalls into .venv
    task test    # runs both C++ and Python tests against the new build

``task test`` calls ``task build`` first, so run it directly.

**Running only the C++ tests**::

    task test-cpp

That builds the C++ test executable — Ninja if installed, Make otherwise —
leaves the Python extension alone, and runs the result under CTest.

**Checking documentation**::

    task docs-serve   # opens at http://127.0.0.1:8000

Project Structure
-----------------

.. code-block:: text

    lunalink/
    ├── cpp/
    │   ├── include/lunalink/ # C++ public headers, one directory per module
    │   ├── codes/            # spreading codes (Gold, Weil, matched code)
    │   ├── encoding/         # BCH, LDPC, CRC-24, interleaver
    │   ├── messages/         # frame assembly and message parsing
    │   ├── signal/           # modulation, I/Q multiplexing, sync, decode
    │   ├── core/             # shared safety and integrity primitives
    │   ├── platform/         # hardware abstraction layer
    │   ├── bindings/afs_module.cpp   # nanobind bindings
    │   ├── fuzz/             # libFuzzer harnesses
    │   └── tests/            # Catch2 tests, mirroring the module directories
    ├── src/lunalink/         # Python package (src layout)
    │   ├── __init__.py       # exposes __version__
    │   ├── cli.py            # the ``lunalink`` command
    │   └── afs/              # AFS Python API + extension stubs
    ├── tests/                # pytest tests
    ├── examples/             # runnable transmit / decode / round-trip scripts
    ├── scripts/              # evidence generators and CI gates
    ├── docs/                 # Sphinx documentation source
    ├── .githooks/            # pre-commit hook
    ├── .github/workflows/    # CI pipeline
    ├── CMakeLists.txt        # C++ build definition
    ├── pyproject.toml        # project metadata and tool configuration
    └── Taskfile.yml          # task definitions

Adding a New C++ Function
--------------------------

1. Declare the function in an appropriate header under ``cpp/include/lunalink/``.
2. Implement it in the matching source file under the module directory
   (``cpp/codes/``, ``cpp/encoding/``, ``cpp/messages/``, ``cpp/signal/``,
   ``cpp/core/`` or ``cpp/platform/``), then add it to the ``lunalink_core`` sources in
   ``CMakeLists.txt``.
3. Expose it via nanobind in ``cpp/bindings/afs_module.cpp``.
4. Add/update the corresponding type stub in ``src/lunalink/afs/_afs.pyi``.
5. Re-export from ``src/lunalink/afs/signal.py`` and update ``__all__``.
6. Write Catch2 tests in ``cpp/tests/`` and pytest tests in ``tests/``.
7. Run ``task test`` to verify everything passes.

Pre-commit Hook
---------------

The hook in ``.githooks/pre-commit`` runs automatically on every
``git commit``. It delegates to ``task pre-commit`` (``Taskfile.yml`` is the
source of truth), which currently runs:

1. build
2. lint-check
3. tidy-staged (clang-tidy on the staged C++ files only)
4. test-cpp
5. test-py

Any failing step blocks the commit. To skip the hook in an emergency
(not recommended)::

    git commit --no-verify

To re-run the checks manually without committing::

    task pre-commit

The hook is the fast subset. Coverage, the sanitizers, full clang-tidy, fault
injection and the docs build are not in it: ``task pre-push`` runs the
coverage gate and the strict docs build, and ``task ci-checks`` runs the
whole gate.

CI Pipeline
-----------

GitHub Actions workflows run on every push to ``main``; the fast gates also
run on every pull request, and the long ones run on ``main`` only, nightly,
or on demand.  Independent workflows keep the status reporting granular.
``task ci-checks`` reproduces the gate locally; the long workflows are
their own tasks.

.. list-table::
   :header-rows: 1
   :widths: 22 14 64

   * - Workflow
     - Trigger
     - Description
   * - **Lint** (``lint.yml``)
     - push, PR
     - ``task lint-check`` (ruff, pyright, cppcheck, docstring sync),
       clang-format check, S10 build-flags gate
       (``scripts/check_build_flags.py``).
   * - **C++ Tests** (``test-cpp.yml``)
     - push, PR
     - CMake + Ninja Debug build; Catch2 unit + integration tests
       (``~[slow]``); the SWIFI binary is built with
       ``LUNALINK_FAULT_INJECTION=ON`` and run.
   * - **Python Tests** (``test-python.yml``)
     - push, PR
     - ``pytest`` on Python 3.12, 3.13 and 3.14 (Ubuntu) plus 3.12 on
       Windows and macOS for pushes to ``main``.  Builds LANS-AFS-SIM so
       the interop tests run, with a gate (G-15) that fails if they
       skip; 100% line-coverage gate; coverage badge.
   * - **Integration Tests** (``integration.yml``)
     - push, PR
     - ``[e2e]``-tagged full TX→RX tests, including the quantisation and
       burst-error impairment tests.
   * - **Acceptance Tests** (``acceptance.yml``)
     - push to main, manual
     - The whole Catch2 binary including ``[slow]``: Monte Carlo Pd, BER,
       channel and E2E suites.
   * - **Sanitizers** (``sanitize.yml``)
     - push to main
     - C++ tests recompiled with ``-fsanitize=address,undefined``; any
       report is a blocking failure.
   * - **C++ Coverage — MC/DC** (``coverage-cpp.yml``)
     - push to main
     - LLVM source-based coverage with MC/DC; thresholds enforced
       (region 90%, line 85%, function 95%, branch 75%, MC/DC 30%);
       coverage badges.
   * - **Clang Tidy** (``tidy.yml``)
     - push, PR
     - clang-tidy on the MISRA-traceable profile, then the Clang Static
       Analyzer (``scan-build``); zero findings required.
   * - **WCET Profile** (``wcet.yml``)
     - push to main, nightly, manual
     - Release build; ``[wcet]`` benchmarks and budgets, the constant-time
       gate and the BER simulations; a 25% CI-spread measurement-quality
       gate; trend history when a ``gh-pages`` branch exists.  Uploads
       ``wcet_profile.rst``.
   * - **Traceability** (``traceability.yml``)
     - push to main
     - ``scripts/verify_traceability.py``: every requirement mapped to a
       test, zero orphaned tags.
   * - **Fuzz Smoke** (``fuzz-smoke.yml``)
     - push, PR
     - Builds the three libFuzzer harnesses and replays the committed seed
       corpus (``-runs=0``).
   * - **Nightly Fuzzing** (``fuzz.yml``)
     - nightly, manual
     - Coverage-guided search per harness, with corpus restore and
       minimisation.
   * - **Cross-Architecture** (``cross-arch.yml``)
     - push to main
     - Docker buildx + QEMU: ``linux/arm64`` and big-endian
       ``linux/s390x`` build and test.
   * - **validate-export** (``validate-export.yml``)
     - push, PR, manual
     - Builds and tests a ``git archive`` of the committed tree, catching
       files present on disk but never committed.
   * - **Docs Check** (``docs-check.yml``)
     - push, PR
     - Doxygen, then the Sphinx build with ``-W`` (warnings as errors).
   * - **Deploy Docs** (``docs-pages.yml``)
     - push to main, manual
     - Assembles the assurance artefacts, embeds the coverage HTML report
       and deploys to GitHub Pages (needs Pages enabled on a public
       repository).
   * - **Recertification** (``recertify.yml``)
     - manual only
     - Every assurance gate against a chosen ref; emits a
       milestone-tagged, signed-off record (see `Recertification`_ below).
   * - **Release** (``release.yml``)
     - ``v*`` tag, manual
     - Builds the source distribution and binary wheels, attaches them to
       the GitHub release and publishes to PyPI.  A manual run exercises
       the build without publishing.

Every CI runner has Ninja, so both the Python extension build (via
scikit-build-core) and the C++ test build pick it up automatically.

Toolchain
---------

The C++ core and the Python layer use separate tool sets.  The tables give
the role of each tool; pinned versions are in ``pyproject.toml``, ``uv.lock``
and ``CMakeLists.txt``.

Flight Software Stack (C++)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LunaLink is audited against **PE-RS-ESA-GS-0871** (ESA Python Coding Guidelines).

Code Style & Naming
^^^^^^^^^^^^^^^^^^^
*   **Mandate**: PEP 8 baseline; ``snake_case`` for functions; ``PascalCase`` for classes.
*   **Compliance**: ``ruff`` enforces these via the ``E``, ``W``, and ``N`` rule sets.

Project Structure
^^^^^^^^^^^^^^^^^
*   **Mandate**: ``src`` layout; explicit ``pyproject.toml``.
*   **Compliance**: all source code sits in ``src/lunalink/``.

Dependency Management
^^^^^^^^^^^^^^^^^^^^^
*   **Mandate**: Virtual environments; exact version pinning.
*   **Compliance**: ``uv sync`` generates a bit-for-bit reproducible environment via ``uv.lock``.

Documentation
^^^^^^^^^^^^^
*   **Mandate**: Docstrings for all public interfaces (PEP 257); Sphinx-based.
*   **Compliance**: NumPy-style docstrings rendered via **Sphinx** with the **Read the Docs** theme.

Type Safety
^^^^^^^^^^^
*   **Mandate**: Type hints (PEP 484) for public APIs.
*   **Compliance**: ``pyright`` enforces strict typing; ``.pyi`` stubs cover the C++ modules.

Testing & Quality
^^^^^^^^^^^^^^^^^
*   **Mandate**: White-box unit testing; Cyclomatic Complexity ≤ 10.
*   **Compliance**:
    *   ``pytest-cov`` enforces the 100% line-coverage gate (the build fails below it).
    *   ``ruff (C90)`` and ``radon cc`` enforce complexity limits.
    *   ``radon hal`` and ``radon mi`` generate ESA-required quality metrics.

Recertification
---------------

Beyond those workflows sits **on-demand recertification**: one authorised
run that exercises every assurance gate against a chosen git ref and emits a
dated, signed-off RST record to attach to a milestone review — PDR, CDR, SAR,
a flight baseline.

**When to use it:**

* Before cutting a formal baseline tag (``v*``).
* To certify a hotfix branch before merging to ``main``.
* As evidence for a programme milestone review (SRR/PDR/CDR/SAR).
* Periodically as a scheduled quality check on long-lived feature branches.

**Running locally** (9 of 10 gates; MC/DC coverage requires dedicated CI
toolchain):

.. code-block:: bash

    task recertify MILESTONE=PDR REASON="Pre-PDR baseline" AUTHORITY="Your Name"

Gates run in sequence, a coloured per-gate summary prints, and
``docs/assurance/recertification_record.rst`` is written.  Any failing gate
makes the exit code non-zero.  Per-gate logs land in
``build/recertify-logs/``.

**Running in CI** (all 10 gates in parallel):

Trigger ``workflow_dispatch`` on ``.github/workflows/recertify.yml`` from
the GitHub Actions UI.  Inputs:

.. list-table::
   :header-rows: 1
   :widths: 18 82

   * - Input
     - Description
   * - ``ref``
     - Branch, tag, or full SHA to certify (e.g. ``main``, ``v1.2.0``,
       ``abc1234``).
   * - ``milestone``
     - Lifecycle phase: ``SRR``, ``PDR``, ``CDR``, ``SAR``, ``Flight``,
       ``Hotfix``, or ``Periodic``.
   * - ``reason``
     - Freetext reason (e.g. ``"Pre-CDR baseline certification"``).
   * - ``authority``
     - Authorising party name or role (appears in the signature block).

All 10 gate jobs run in **parallel** regardless of each other's outcome —
a failure in one never cancels the others.  The ``record`` job always runs
last (``if: always()``), collects every gate outcome, and uploads the
artifact:

.. code-block:: text

    recertification-record-<MILESTONE>-<short-SHA>

**The record document** (``docs/assurance/recertification_record.rst``)
contains:

* Document ID, certified ref (branch + full SHA), milestone, timestamp,
  authorising party.
* Gate verdict table with specification references (SWE-108, SW-SAF-06, …).
* **Overall verdict**: ``PASS`` when all gates pass; ``FAIL`` when any gate
  fails; ``WARN`` when gates were skipped or cancelled.
* Open items list for any non-``PASS`` gates.
* Blank signature block for Lead Developer / IV&V / Programme Manager.

To commit the record alongside a release tag::

    task recertify MILESTONE=Flight REASON="Flight baseline v1.0.0" AUTHORITY="M. Rossi"
    git add docs/assurance/recertification_record.rst
    git commit -m "assurance: add Flight recertification record for v1.0.0"
    git tag -s v1.0.0

**Reuse in other projects:**  Copy ``scripts/gen_recertification_record.py``,
``scripts/recertify_local.sh``, and ``.github/workflows/recertify.yml`` to
any project.  The generator has no project-specific knowledge.  Update the
``GATE_KEYS`` / ``GATE_LABELS`` / ``GATE_COMMANDS`` in ``recertify_local.sh``
and the gate jobs in ``recertify.yml`` to match the target project's gate set.
