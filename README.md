# lsis-afs

LSIS AFS Python package with C++ extensions via pybind11.

## Setup

```bash
uv sync --group dev
uv pip install -e .
```

## Development

Run tests with coverage:
```bash
uv run pytest
```

Type checking:
```bash
uv run ty check src/
```

Lint and format:
```bash
uv run ruff check src/ tests/
uv run ruff format src/ tests/
```
