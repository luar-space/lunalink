Flight vs Non-Flight Build Policy
=================================

Purpose
-------
Define the build policy that distinguishes flight code from non-flight code,
including required compiler flags and prohibited features.

Flight Code Scope
-----------------
Flight code includes:
- ``cpp/{codes,encoding,messages,signal,platform}/`` implementation sources
- ``cpp/include/lunalink/signal/`` public headers

Non-flight code includes:
- Tests, tools, benchmarks, and host utilities

Flight Build Requirements
-------------------------
1. Flight targets must compile with ``-fno-exceptions`` and ``-fno-rtti``.
2. Forbidden optimization flags must not appear in flight builds.
3. Flight targets must enable hardening flags (stack protection, FP stability).
4. Flight builds must produce deterministic binaries for identical inputs.

Non-Flight Build Policy
-----------------------
Non-flight targets may enable broader C++ features when explicitly configured,
but must never be linked into flight binaries.

Enforcement
-----------
- CI must enforce a build gate that verifies flight targets use the required
  flags and avoid forbidden flags.
- CI must reject usage of ``throw``, ``try``/``catch``, ``dynamic_cast``, and
  ``typeid`` in flight directories.
- The compile commands database (``compile_commands.json``) is the source of
  truth for flag verification.
