Tech Stack and ESA Compliance
==============================

This document describes the tools chosen for this project and maps each one
to the relevant requirements from the ESA Python coding guidelines
(PE-RS-ESA-GS-0871) and the ESA guidelines for Python development
(PE-TN-ESA-GS-0872).

Tech Stack
----------

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Tool
     - Version
     - Role
   * - `uv <https://docs.astral.sh/uv/>`_
     - ≥ 0.9
     - Dependency management, virtual environment, package build
   * - `scikit-build-core <https://scikit-build-core.readthedocs.io/>`_
     - ≥ 0.9
     - Build backend for C++ extensions (replaces setuptools)
   * - `pybind11 <https://pybind11.readthedocs.io/>`_
     - ≥ 2.13
     - C++/Python bindings
   * - `Catch2 <https://github.com/catchorg/Catch2>`_
     - v3.6.0
     - C++ unit testing framework (fetched automatically by CMake)
   * - `clang-tidy <https://clang.llvm.org/extra/clang-tidy/>`_
     - system (≥ 14)
     - C++ static analysis: ``bugprone-*``, ``cert-*``, ``cppcoreguidelines-*``, ``hicpp-*``, ``fuchsia-*``
   * - `gcovr <https://gcovr.com/>`_
     - ≥ 7.0
     - C++ line coverage HTML report generation and threshold enforcement (≥ 90%)
   * - `pytest <https://docs.pytest.org/>`_
     - ≥ 8.0
     - Unit testing framework
   * - `pytest-cov <https://pytest-cov.readthedocs.io/>`_
     - ≥ 5.0
     - Test coverage measurement and reporting
   * - `ruff <https://docs.astral.sh/ruff/>`_
     - ≥ 0.9
     - Linting (PEP 8, naming, docstrings, complexity) and formatting
   * - `pyright <https://microsoft.github.io/pyright/>`_
     - ≥ 1.1
     - Static type checking
   * - `radon <https://radon.readthedocs.io/>`_
     - ≥ 6.0
     - Code quality metrics (cyclomatic complexity, Halstead, maintainability index)
   * - `Sphinx <https://www.sphinx-doc.org/>`_
     - ≥ 7.0
     - API documentation generation from docstrings
   * - `furo <https://pradyunsg.me/furo/>`_
     - ≥ 2024.1
     - Sphinx HTML theme

ESA Guidelines Compliance
--------------------------

Code Style
~~~~~~~~~~

**Requirement:** PEP 8 is the baseline style standard.

**Implementation:** ``ruff`` enforces PEP 8 via the ``E`` (pycodestyle errors)
and ``W`` (pycodestyle warnings) rule sets. Formatting is applied automatically
with ``task lint``.

Naming Conventions
~~~~~~~~~~~~~~~~~~

**Requirement:** ``snake_case`` for variables, functions, and modules;
``PascalCase`` for class names.

**Implementation:** ``ruff`` rule set ``N`` (pep8-naming) enforces these
conventions and reports violations as lint errors.

Project Structure
~~~~~~~~~~~~~~~~~

**Requirement:** The ``src`` layout is mandated. Projects must include a
``pyproject.toml``, a ``README``, and a ``LICENSE``.

**Implementation:** Source code lives under ``src/lunalink/``. A
``pyproject.toml`` and ``README.md`` are present at the project root.


Dependency Management
~~~~~~~~~~~~~~~~~~~~~

**Requirement:** Virtual environments are mandatory. Dependencies must be
version-pinned to exact versions.

**Implementation:** ``uv`` creates and manages a ``.venv`` virtual environment
automatically. Exact versions for all dependencies are recorded in
``uv.lock``, which is committed to the repository and used to reproduce the
environment deterministically via ``uv sync``.

Documentation
~~~~~~~~~~~~~

**Requirement:** Docstrings are required for all public modules, classes,
functions, and methods (PEP 257). Docstring formats compatible with Sphinx
are recommended; the ESA guidelines explicitly recommend Sphinx as the
documentation tool.

**Implementation:** NumPy-style docstrings are enforced by ``ruff`` rule set
``D`` with ``convention = "numpy"``. Sphinx with the ``napoleon`` extension
renders these docstrings into HTML API documentation. The ``sphinx.ext.viewcode``
extension links documentation pages back to the source.

Type Annotations
~~~~~~~~~~~~~~~~

**Requirement:** Type hints (PEP 484) are recommended for all public interfaces.

**Implementation:** Type hints are required on all public function signatures.
``pyright`` performs static type analysis and is run as part of ``task lint`` and
the CI pipeline. Type stubs (``.pyi`` files) are provided for compiled C++
extension modules.

Logging
~~~~~~~

**Requirement:** The standard library ``logging`` module must be used.
``print()`` statements must not be used for error reporting.

**Implementation:** Every module obtains a logger via
``logging.getLogger(__name__)``. No ``print()`` calls are permitted for
diagnostics; ``ruff`` rule ``T20`` can be added to enforce this if needed.

Testing
~~~~~~~

**Requirement:** Unit tests are required. Statement coverage must be
achievable via white-box unit testing (ECSS-E-ST-40C §5.8.3.5).

**Implementation:** ``pytest`` is used for unit testing. ``pytest-cov``
measures and reports both statement and branch coverage after every test run.
A minimum of 90% coverage is enforced via ``--cov-fail-under=90`` in
``pyproject.toml``.

Cyclomatic Complexity
~~~~~~~~~~~~~~~~~~~~~

**Requirement:** Maximum cyclomatic complexity of 10 per function
(ECSS/LDRA guideline).

**Implementation:** ``ruff`` rule set ``C90`` (mccabe) is enabled with
``max-complexity = 10``. Any function exceeding this threshold is reported
as a lint error. ``radon cc`` provides per-function complexity scores
(``task metrics``) for detailed reporting in software quality deliverables.

Code Quality Metrics
~~~~~~~~~~~~~~~~~~~~

**Requirement:** ESA recommends computing Halstead metrics and a
maintainability index as part of software quality reporting.

**Implementation:** ``radon`` computes all three metric families:

- **CC** — cyclomatic complexity per function (grade A–F)
- **MI** — maintainability index (0–100 composite score)
- **HAL** — Halstead volume, difficulty, and estimated bug count

Run ``task metrics`` to generate a report.

CI/CD
-----

A GitHub Actions workflow (``.github/workflows/ci.yml``) runs on every push
and pull request to ``main`` and on all version tags (``v*``). It executes
nine independent jobs in parallel:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Job
     - Steps
   * - **Lint**
     - ``ruff format --check``, ``ruff check``, ``pyright src/``
   * - **Test × 3**
     - ``pytest`` with statement + branch coverage (≥ 90% required), on Python 3.12, 3.13, and 3.14
   * - **C++ Tests**
     - CMake + Ninja, CTest
   * - **Sanitizers**
     - C++ tests with ``-fsanitize=address,undefined``; detects memory errors and UB at runtime
   * - **clang-tidy**
     - Static analysis of C++ core and tests; all findings treated as errors
   * - **C++ Coverage**
     - gcov + gcovr HTML report; ≥ 90% C++ line coverage enforced
   * - **Docs**
     - ``sphinx-build -W`` (warnings treated as errors)

A pre-commit hook (``.githooks/pre-commit``) mirrors these checks locally.
Activate it with ``task install-hooks``.
