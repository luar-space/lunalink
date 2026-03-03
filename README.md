# lsis-afs

LSIS AFS Python package with C++ extensions via pybind11.

## Prerequisites

| Tool | Install | Purpose |
|------|---------|---------|
| [uv](https://docs.astral.sh/uv/) | `curl -LsSf https://astral.sh/uv/install.sh \| sh` | Python environment and package management |
| [Task](https://taskfile.dev) | `brew install go-task` | Task runner |
| [CMake](https://cmake.org) ≥ 3.15 | `brew install cmake` | C++ build system |
| [Ninja](https://ninja-build.org) | `brew install ninja` | Fast C++ build backend (optional) |
| C++17 compiler | `xcode-select --install` (macOS) | Required for C++ extension |

## Setup

```bash
git clone <repo-url>
cd lsis-afs
task install        # create .venv and install all dependencies
task install-hooks  # register the pre-commit hook
```

## Common Commands

```bash
task           # full quality pipeline: lint → test → docs
task test      # rebuild C++ extension, run C++ tests, run pytest
task lint      # type check (ty) + format + lint (ruff)
task build     # force-recompile the C++ extension
task test-cpp  # build and run C++ Catch2 tests only
task metrics   # cyclomatic complexity and maintainability index (radon)
task docs-build  # build Sphinx HTML docs → docs/_build/html/
task docs-serve  # serve docs locally with live reload at http://127.0.0.1:8000
```

## Versioning

Versions are derived automatically from git tags using `setuptools-scm`.
To cut a release:

```bash
git tag v1.2.3
git push origin v1.2.3
```

See `docs/versioning.rst` for full details.

## Documentation

```bash
task docs-serve   # opens at http://127.0.0.1:8000
```
