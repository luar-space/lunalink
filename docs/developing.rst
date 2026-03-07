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
    cd lunalink
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

    # edit src/lunalink/...
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

    lunalink/
    ├── cpp/
    │   ├── include/lunalink/signal/  # C++ public headers
    │   ├── signal/                   # C++ implementation (no Python dependency)
    │   ├── bindings/afs_module.cpp   # pybind11 bindings
    │   └── tests/            # Catch2 unit tests
    ├── src/lunalink/         # Python package (src layout)
    │   ├── __init__.py       # exposes __version__
    │   └── afs/              # AFS Python API + extension stubs
    ├── tests/                # pytest tests
    ├── docs/                 # Sphinx documentation source
    ├── .githooks/            # pre-commit hook
    ├── .github/workflows/    # CI pipeline
    ├── CMakeLists.txt        # C++ build definition
    ├── pyproject.toml        # project metadata and tool configuration
    └── Taskfile.yml          # task definitions

Adding a New C++ Function
--------------------------

1. Declare the function in an appropriate header under ``cpp/include/lunalink/``.
2. Implement it in a corresponding source file under ``cpp/signal/`` (or other
   module directory), and ensure it is included in ``lunalink_core`` sources in
   ``CMakeLists.txt``.
3. Expose it via pybind11 in ``cpp/bindings/afs_module.cpp``.
4. Add/update the corresponding type stub in ``src/lunalink/afs/_afs.pyi``.
5. Re-export from ``src/lunalink/afs/signal.py`` and update ``__all__``.
6. Write Catch2 tests in ``cpp/tests/`` and pytest tests in ``tests/``.
7. Run ``task test`` to verify everything passes.

Pre-commit Hook
---------------

The hook in ``.githooks/pre-commit`` runs automatically on every
``git commit``. It delegates to ``task ci-checks`` (source of truth), which
currently runs:

1. build
2. lint-check
3. test-cpp
4. coverage-cpp
5. tidy
6. test-py
7. sanitize
8. docs-build

The commit is blocked if any step fails. To skip the hook in an emergency
(not recommended)::

    git commit --no-verify

To re-run the checks manually without committing::

    task lint
    task test
    task docs-build

CI Pipeline
-----------

GitHub Actions workflows run on every push to ``main``, on every tag 
matching ``v*``, and on all pull requests. The pipeline is split into 
independent workflows for granular status reporting:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Workflow
     - Description
   * - **Python Tests**
     - ``pytest`` run on Python 3.12, 3.13, and 3.14 with coverage enforcement (≥ 100%).
   * - **Lint**
     - ``ruff format --check``, ``ruff check``, and ``pyright`` type checking.
   * - **C++ Tests**
     - CMake + Ninja build and execution of Catch2 unit tests.
   * - **Sanitizers**
     - C++ tests recompiled with ``-fsanitize=address,undefined`` to catch UB at runtime.
   * - **clang-tidy**
     - Static analysis of C++ core sources for MISRA/High-Integrity compliance.
   * - **C++ Coverage**
     - gcov instrumentation and reporting; ≥ 90% line coverage enforced.
   * - **Docs Check**
     - Verification of Sphinx documentation build (warnings as errors).
   * - **Docs Deploy**
     - Automatic deployment of documentation to GitHub Pages upon merge to ``main``.

Ninja is installed on all CI runners so both the Python extension build
(via scikit-build-core) and the C++ test build use it automatically.
