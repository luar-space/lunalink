Versioning
==========

Version numbers are derived automatically from git tags using
`setuptools-scm <https://setuptools-scm.readthedocs.io/>`_. There is no
version string to maintain manually in the source code.

Scheme
------

This project follows `Semantic Versioning <https://semver.org/>`_ (SemVer):
``MAJOR.MINOR.PATCH``.

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Segment
     - When to increment
   * - ``MAJOR``
     - Incompatible API changes
   * - ``MINOR``
     - New functionality, backwards-compatible
   * - ``PATCH``
     - Bug fixes, backwards-compatible

Between tagged releases, setuptools-scm produces a development version of
the form ``MAJOR.MINOR.PATCH.devN+gHASH`` where ``N`` is the number of
commits since the last tag and ``HASH`` is the short git commit hash.

Cutting a Release
-----------------

1. Ensure the working tree is clean and all checks pass::

      task lint
      task test
      task docs-build

2. Create and push an annotated tag::

      git tag -a v1.2.3 -m "Release 1.2.3"
      git push origin v1.2.3

3. Rebuild the package to confirm the version resolves correctly::

      task build
      uv run python -c "import lunalink; print(lunalink.__version__)"

The version is now ``1.2.3`` everywhere: the installed package metadata,
the Sphinx documentation, and any wheel built for distribution.

How It Works
------------

``setuptools-scm`` is listed as a build-system dependency in
``pyproject.toml`` and is invoked by scikit-build-core at build time via
the ``scikit_build_core.metadata.setuptools_scm`` metadata provider::

   [build-system]
   requires = ["scikit-build-core>=0.9", "pybind11>=2.13", "setuptools-scm"]

   [project]
   dynamic = ["version"]

   [tool.scikit-build]
   metadata.version.provider = "scikit_build_core.metadata.setuptools_scm"

   [tool.setuptools_scm]
   fallback_version = "0.0.0.dev0"

The ``fallback_version`` is used when building outside of a git repository
(e.g. from a source tarball without git history).

The Sphinx documentation reads the version from the installed package
metadata at build time::

   from importlib.metadata import PackageNotFoundError
   from importlib.metadata import version as get_version

   try:
       release = get_version("lunalink")
   except PackageNotFoundError:
       release = "unknown"
