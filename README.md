# LunaLink

[![CI](https://github.com/fiskio/lunalink/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/fiskio/lunalink/actions/workflows/ci.yml)
[![Docs Deploy](https://github.com/fiskio/lunalink/actions/workflows/docs-pages.yml/badge.svg?branch=main)](https://github.com/fiskio/lunalink/actions/workflows/docs-pages.yml)
[![Docs Site](https://img.shields.io/badge/docs-github%20pages-blue)](https://fiskio.github.io/lunalink/)
[![C++ Tests](https://github.com/fiskio/lunalink/actions/workflows/ci.yml/badge.svg?branch=main&job=C%2B%2B%20Tests)](https://github.com/fiskio/lunalink/actions/workflows/ci.yml)
[![Sanitizers](https://github.com/fiskio/lunalink/actions/workflows/ci.yml/badge.svg?branch=main&job=Sanitizers%20(ASan%20%2B%20UBSan))](https://github.com/fiskio/lunalink/actions/workflows/ci.yml)
[![clang-tidy](https://github.com/fiskio/lunalink/actions/workflows/ci.yml/badge.svg?branch=main&job=clang-tidy)](https://github.com/fiskio/lunalink/actions/workflows/ci.yml)
[![C++ Coverage](https://github.com/fiskio/lunalink/actions/workflows/ci.yml/badge.svg?branch=main&job=C%2B%2B%20Coverage)](https://github.com/fiskio/lunalink/actions/workflows/ci.yml)
[![Last Commit](https://img.shields.io/github/last-commit/fiskio/lunalink/main)](https://github.com/fiskio/lunalink/commits/main)
[![Python](https://img.shields.io/badge/python-3.12%20%7C%203.13%20%7C%203.14-blue.svg)](https://github.com/fiskio/lunalink/blob/main/.github/workflows/ci.yml#L24)
[![Style: ruff](https://img.shields.io/badge/style-ruff-D7FF64?logo=ruff)](https://github.com/astral-sh/ruff)
[![Type checked: pyright](https://img.shields.io/badge/type%20checked-pyright-2A6DB2)](https://github.com/microsoft/pyright)
[![License: Apache 2.0](https://img.shields.io/badge/license-Apache%202.0-green.svg)](https://github.com/fiskio/lunalink/blob/main/LICENSE)

LunaLink AFS Python package with C++ extensions via pybind11.

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
task lint      # type check (pyright) + format + lint (ruff)
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
