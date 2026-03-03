"""LunaLink Python package."""

from importlib.metadata import PackageNotFoundError
from importlib.metadata import version as _get_version

__all__ = ["__version__"]

try:
    __version__: str = _get_version("lunalink")
except PackageNotFoundError:  # pragma: no cover
    __version__ = "unknown"
