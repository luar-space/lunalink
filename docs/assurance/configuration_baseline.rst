Configuration Baseline and Reproducible Builds
==============================================

Purpose
-------
Define how a release build is frozen, reproduced, and verified. The baseline
ensures that certification evidence is tied to an immutable configuration.

Baseline Requirements
---------------------
1. Each release is tagged with a versioned git tag (e.g., ``vX.Y.Z``).
2. The exact compiler, flags, and toolchain versions used for the release
   must be recorded in the release record.
3. The full set of evidence artifacts (coverage, static analysis, SWIFI)
   must be archived and linked to the release tag.
4. The build must be reproducible from a clean checkout using documented
   commands.

Baseline Record Contents
------------------------
- Git tag and commit SHA
- Toolchain versions and invocations
- Build configuration (flight/non-flight)
- Evidence artifact checksums and locations

Reproducibility Procedure
-------------------------
1. Clean checkout at the release tag.
2. Run the documented build command for flight targets.
3. Regenerate evidence artifacts via the analysis command.
4. Verify that artifact hashes match the baseline record.

Notes
-----
The baseline record may be stored in ``recertification_record.rst``
(generated on demand by ``task recertify``; not shipped in this release) or
an explicit release note and must be updated for each release.
