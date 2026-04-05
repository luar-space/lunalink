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
    "nbsphinx",
]

# nbsphinx: notebooks are pre-executed by `task docs-build` so figures render
# without needing a build-time kernel. Notebooks live in docs/notebooks/.
nbsphinx_execute = "never"

# Suppress warnings for forward-references to notebooks not yet written
suppress_warnings = ["nbsphinx.localfile", "ref.python"]

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

exclude_patterns = [
    "_build",
    "Thumbs.db",
    ".DS_Store",
    "assurance/public/README.rst",
    "assurance/README.rst",
    # [[private-begin]]
    # Private assurance docs are excluded from OSS Sphinx builds.
    # [[private-end]]
]

html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]
html_css_files = ["custom.css"]

# -- LaTeX / PDF output -------------------------------------------------------

latex_documents = [
    (
        "gateway0",  # source start file (without .rst)
        "LunaLink-Gateway0-Phase1.tex",  # output filename
        "LSIS-AFS Reference Implementation\n"
        "Standards Compliance \\& Architecture",  # title
        "LunaLink",  # author
        "howto",  # document class: 'howto' for article, 'manual' for report
        False,  # toctree_only
    ),
]

latex_engine = "xelatex"

latex_elements = {
    "papersize": "a4paper",
    "pointsize": "11pt",
    "preamble": r"""
\usepackage{enumitem}
\setlistdepth{9}
\usepackage{tikz}
\usetikzlibrary{arrows.meta, positioning, calc}
""",
    "maketitle": r"""
\begin{titlepage}
\centering
\vspace*{3cm}
{\Huge\bfseries LSIS-AFS Reference Implementation\par}
\vspace{0.5cm}
{\LARGE Standards Compliance \& Architecture\par}
\vspace{1.5cm}
{\Large Phase 1 Deliverable\par}
{\large ESA-CCSDS Protocol Development Competition\par}
\vspace{1cm}
{\large Milestone 1 — 31 March 2026\par}
\vspace{3cm}
{\large\bfseries LunaLink\par}
\vspace{1cm}
{\normalsize Document ID: LUNALINK-GW0-001 \quad Version: 1.0\par}
\vfill
\end{titlepage}
""",
    "tableofcontents": r"\tableofcontents\clearpage",
}
