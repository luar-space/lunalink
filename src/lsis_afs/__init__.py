"""LSIS AFS Python package."""

from importlib.metadata import PackageNotFoundError
from importlib.metadata import version as _get_version

try:
    __version__: str = _get_version("lsis-afs")
except PackageNotFoundError:  # pragma: no cover
    __version__ = "unknown"
