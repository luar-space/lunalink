"""Sphinx configuration for lsis-afs documentation."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

project = "lsis-afs"
author = "LSIS"
release = "0.1.0"

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

html_theme = "furo"
