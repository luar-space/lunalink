Developer Guide
===============

This page covers everything needed to work on this repository, from first
checkout to cutting a release.

Prerequisites
-------------

The following tools must be installed before working on the project:

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
   * - A C++17 compiler
     - Ships with Xcode CLT on macOS (``xcode-select --install``)
     - Required to compile the C++ extension

Getting Started
---------------

Clone the repository and run the one-time setup::

    git clone <repo-url>
    cd lsis-afs
    task install        # create .venv and install all dependencies
    task install-hooks  # register the pre-commit hook

After this, ``task`` (with no arguments) runs the full quality pipeline:
lint → test → docs.

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
     - Build and run only the C++ Catch2 tests via CTest.
   * - ``task coverage-cpp``
     - Build and run C++ tests with gcov, then generate HTML report via gcovr.
   * - ``task lint``
     - Run ``pyright`` (type checking) and ``ruff`` (formatting + linting).
   * - ``task metrics``
     - Report cyclomatic complexity and maintainability index via radon.
   * - ``task docs-build``
     - Build the Sphinx HTML documentation to ``docs/_build/html/``.
   * - ``task docs-serve``
     - Serve the documentation locally with live reload on port 8000.
   * - ``task install-hooks``
     - Register the git pre-commit hook in ``.githooks/``.

Daily Workflow
--------------

**Editing Python code**

No rebuild is needed. Changes to ``src/`` are picked up immediately since
the package is installed in editable mode::

    # edit src/lsis_afs/...
    task test    # runs build (no-op if C++ unchanged), then all tests
    task lint    # type check + format + lint

**Editing C++ code**

After editing any file under ``cpp/``, a rebuild is required::

    # edit cpp/...
    task build   # recompiles the extension and reinstalls into .venv
    task test    # runs both C++ and Python tests against the new build

``task test`` always calls ``task build`` first, so running it directly is
sufficient.

**Running only the C++ tests**::

    task test-cpp

This builds the C++ test executable (using Ninja if installed, Make
otherwise) without touching the Python extension, and runs it under CTest.

**Checking documentation**::

    task docs-serve   # opens at http://127.0.0.1:8000

Project Structure
-----------------

.. code-block:: text

    lsis-afs/
    ├── cpp/
    │   ├── include/          # C++ headers
    │   ├── example_core.cpp  # C++ implementation (no Python dependency)
    │   ├── example.cpp       # pybind11 bindings
    │   └── tests/            # Catch2 unit tests
    ├── src/lsis_afs/         # Python package (src layout)
    │   ├── __init__.py       # exposes __version__
    │   ├── _example.pyi      # type stubs for the compiled extension
    │   ├── example.py        # Python wrapper for C++ bindings
    │   └── py.typed          # PEP 561 marker
    ├── tests/                # pytest tests
    ├── docs/                 # Sphinx documentation source
    ├── .githooks/            # pre-commit hook
    ├── .github/workflows/    # CI pipeline
    ├── CMakeLists.txt        # C++ build definition
    ├── pyproject.toml        # project metadata and tool configuration
    └── Taskfile.yml          # task definitions

Adding a New C++ Function
--------------------------

1. Declare the function in ``cpp/include/example.hpp``.
2. Implement it in ``cpp/example_core.cpp`` (or a new ``_core.cpp`` file,
   added to ``CMakeLists.txt`` as a source for ``lsis_afs_core``).
3. Expose it via pybind11 in ``cpp/example.cpp``.
4. Add the corresponding entry to ``src/lsis_afs/_example.pyi`` (type stub).
5. Re-export from ``src/lsis_afs/example.py`` and update ``__all__``.
6. Write Catch2 tests in ``cpp/tests/`` and pytest tests in ``tests/``.
7. Run ``task test`` to verify everything passes.

Pre-commit Hook
---------------

The hook in ``.githooks/pre-commit`` runs automatically on every
``git commit``. It executes, in order:

1. ``uv sync`` — ensure the environment is up to date
2. ``pyright src/`` — type checking
3. ``ruff format --check`` + ``ruff check`` — formatting and linting (check-only; run ``task lint`` to auto-fix)
4. C++ Catch2 tests via CMake + CTest
5. ``pytest`` — Python tests with coverage
6. ``sphinx-build -W`` — documentation build (warnings are errors)

The commit is blocked if any step fails. To skip the hook in an emergency
(not recommended)::

    git commit --no-verify

To re-run the checks manually without committing::

    task lint
    task test
    task docs-build

CI Pipeline
-----------

The GitHub Actions workflow (``.github/workflows/ci.yml``) runs on every
push to ``main``, on every tag matching ``v*``, and on all pull requests.
It runs nine jobs in parallel:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Job
     - Steps
   * - **Lint**
     - ``ruff format --check``, ``ruff check``, ``pyright src/`` (Python 3.12)
   * - **Test × 3**
     - ``pytest`` with coverage (≥ 90% required), run on Python 3.12, 3.13, and 3.14
   * - **C++ Tests**
     - CMake + Ninja configure, build, CTest
   * - **Sanitizers**
     - C++ tests recompiled with ``-fsanitize=address,undefined``; any memory error or UB aborts with a diagnostic
   * - **clang-tidy**
     - Static analysis of ``example_core.cpp`` and tests; ``bugprone-*``, ``cert-*``, ``cppcoreguidelines-*``, ``hicpp-*``, ``fuchsia-*`` checks
   * - **C++ Coverage**
     - gcov instrumentation, lcov report; ≥ 90% line coverage enforced
   * - **Docs**
     - ``sphinx-build -W`` (warnings treated as errors)

Ninja is installed on all CI runners so both the Python extension build
(via scikit-build-core) and the C++ test build use it automatically.
