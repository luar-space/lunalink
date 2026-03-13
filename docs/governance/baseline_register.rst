Configuration Baseline Register
================================

**Document ID**: LUNALINK-BASELINE-001
**Specification Reference**: ECSS-Q-ST-80C Rev.2 §5.4; NPR 7150.2D SWE-077 (secondary)
**Revision**: 1.0
**Date**: 2026-03-12
**Status**: Released — Competition Submission Baseline

Purpose
-------

This register is the authoritative record of formal configuration baselines
established during LunaLink development.  Each baseline maps a git tag to a
lifecycle phase exit, records the approving authority, and captures the SHA
of the tagged commit.

A baseline is established whenever a gateway exit is declared and its
verification criteria are met (per ``governance/sdp.rst`` §Entry/Exit
Criteria Summary).  Baselines may only be added to this register via a pull
request to ``main`` that has been approved by the Lead Developer and has
passed all CI gates.

.. note::

   Subsequent baselines are added per gateway exit via PR to this file.
   No baseline may be retroactively modified; only new rows may be appended.

Baseline Register
------------------

.. list-table::
   :header-rows: 1
   :widths: 10 16 24 12 18 15 25

   * - Baseline ID
     - Git Tag
     - SHA (full)
     - Date
     - Lifecycle Phase
     - Approved By
     - Notes
   * - BL-001
     - ``v0.1.0``
     - ``a397ed51d3fcedffef9b5dfb20ab1bd8135974df``
     - 2026-03-12
     - Gateway 1 Exit
     - Competition Submission
     - Spreading code generators: Gold-2046, Weil-10230, Weil-1500,
       matched-code combination, secondary codes.  All SWIFI, MC/DC,
       and static-analysis gates green.
   * - BL-002
     - ``v0.2.0-gw2``
     - ``6e2bdacc53c6118709bb943714f9348e34a3a009``
     - 2026-03-12
     - Gateway 2 Exit
     - Competition Submission
     - FEC complete: BCH(51,8) encode/decode, LDPC(1200,600) encode,
       60×98 block interleaver, CRC-24Q.  Assurance document suite
       (FMEA, Safety Plan, SCMP, SDP, OSS assessment, certification
       delta, and this register) added and baselined.

.. note::

   The SHA for BL-002 reflects the HEAD of the ``assurance`` branch at
   the time this document was authored.  The SHA will be updated to the
   final merge-to-``main`` SHA by the gateway exit PR.

Deviation Register
-------------------

A deviation is an approved departure from a process or product requirement.
All deviations must be recorded here with the baseline against which they
apply, a description, the approving authority, and the resolution.

.. list-table::
   :header-rows: 1
   :widths: 10 12 38 20 20

   * - Dev ID
     - Baseline
     - Deviation Description
     - Approved By
     - Resolution
   * - *(none)*
     - —
     - No deviations recorded at this baseline.
     - —
     - —

.. note::

   All compensating controls and self-certification scopes acknowledged in
   the competition context are documented in ``certification_delta.rst``,
   not here.  This register covers process deviations only.

Change Authority
-----------------

The following change authority rules apply to all baselines recorded in this
register:

1. **Pull requests to ``main``** require:

   * At least **1 approving reviewer** (other than the PR author).
   * All CI gates green (lint, test-py, test-cpp, sanitize, clang-tidy,
     coverage-cpp, docs).
   * No open S1 PRACA issues.

2. **S1 safety fixes** additionally require:

   * At least **2 approving reviewers**, one of whom must be the Lead Developer.
   * SWIFI suite 100% pass and MC/DC 100% pass on the fix branch before
     merge is attempted.

3. **Baseline establishment** (appending a row to this register) requires:

   * Verification phase exit criteria fully met (per ``sdp.rst``).
   * Golden test vectors committed to ``tests/vectors/vX.Y/``.
   * Software Version Description (``assurance/svd.rst``) populated and
     committed for the tag.
   * Lead Developer approval on the baseline PR.

4. **Emergency hot-fixes** (S1 defect discovered post-baseline):

   * A new patch-level tag is created (e.g., ``v0.2.1``).
   * The hot-fix must satisfy all S1 fix-verification requirements in
     ``assurance/praca.rst`` §Fix Verification Gate.
   * A new BL row is appended to this register for the patch tag.
   * The SVD for the affected baseline is superseded by the patch SVD.
