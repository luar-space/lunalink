"""Sphinx configuration for lunalink documentation."""

import sys
from importlib.metadata import PackageNotFoundError
from importlib.metadata import version as get_version
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

_REPO = Path(__file__).parents[1]


def _metric_substitutions() -> str:
    """Build RST substitutions for measured metrics.

    Coverage figures are read from the generated evidence artefact rather than
    transcribed into prose, so a regenerated ``coverage_summary.json`` updates
    every page that cites it.  ``scripts/check_doc_metrics.py`` enforces the
    same rule for ``README.md``, which is Markdown and cannot use
    substitutions.
    """
    import json

    summary = _REPO / "docs" / "assurance" / "evidence" / "coverage_summary.json"
    try:
        totals = json.loads(summary.read_text())["totals"]
    except (OSError, KeyError, ValueError):
        # Docs must still build in a tree with no coverage run yet.
        return "".join(
            f".. |cov-{k}| replace:: *(not measured)*\n"
            for k in ("line", "region", "branch", "mcdc", "function")
        )

    def pct(key: str) -> str:
        return f"{totals[key]['percent']:.2f}%"

    return (
        f".. |cov-line| replace:: {pct('lines')}\n"
        f".. |cov-region| replace:: {pct('regions')}\n"
        f".. |cov-branch| replace:: {pct('branches')}\n"
        f".. |cov-mcdc| replace:: {pct('mcdc')}\n"
        f".. |cov-function| replace:: {pct('functions')}\n"
    )


rst_prolog = _metric_substitutions()

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
    # Folder index for people browsing the repo tree; the Sphinx navigation
    # for these documents is the "Mission Assurance" toctree in index.rst.
    "assurance/README.rst",
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
