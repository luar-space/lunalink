"""Sphinx configuration for lunalink documentation."""

import sys
from importlib.metadata import PackageNotFoundError
from importlib.metadata import version as get_version
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

project = "lunalink"
author = "LunaLink"

try:
    release = get_version("lunalink")
except PackageNotFoundError:
    release = "unknown"

version = release

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.napoleon",
    "sphinx.ext.viewcode",
    "sphinx.ext.intersphinx",
]

napoleon_numpy_docstring = True
napoleon_google_docstring = False

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
}

autodoc_default_options = {
    "members": True,
    "undoc-members": False,
    "show-inheritance": True,
}

exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

html_theme = "furo"
