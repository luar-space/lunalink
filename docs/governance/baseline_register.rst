Configuration Baseline Register
================================

**Document ID**: LUNALINK-BASELINE-001
**Specification Reference**: ECSS-Q-ST-80C Rev.2 §5.4; NPR 7150.2D SWE-077 (secondary)
**Revision**: 1.0
**Date**: 2026-03-12
**Status**: Released — Competition Submission Baseline

Purpose
-------

The authoritative record of formal configuration baselines.  Each row maps a
git tag to a lifecycle phase exit, names the approving authority, and captures
the SHA of the tagged commit.

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
     - FEC complete: BCH(51,8) encode/decode, LDPC SF2 (2400,1200) /
       SF3–SF4 (1740,870) encode,
       60×98 block interleaver, CRC-24Q.  Assurance document suite
       (FMEA, Safety Plan, SCMP, SDP, OSS assessment, certification
       delta, and this register) added and baselined.
   * - BL-003
     - ``v1.0-submission``
     - *(recorded at tagging)*
     - 2026-08-31
     - Competition Submission
     - Lead Developer
     - Competition submission baseline — final deliverable tag for the
       ESA-CCSDS competition.  SHA recorded at tagging.

.. note::

   **Provenance.**  This repository is a squashed public export of the
   development tree prepared for the competition submission.  The historic
   per-gateway commit SHAs recorded for BL-001 and BL-002 refer to the
   development tree and do not exist in this repository's history; they are
   retained verbatim as the historical record.  BL-002's SHA reflects the
   HEAD of the development ``assurance`` branch at the time the row was
   authored and is final for this register — the "will be updated at
   gateway exit" provision is resolved by this export.  BL-003 is the
   first baseline cut directly from this repository.

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
   the competition context are documented in the SDP (``docs/governance/sdp.rst``),
   not here.  This register covers process deviations only.

Change Authority
-----------------

Four rules govern change authority for every baseline in this register:

1. **Pull requests to ``main``** require:

   * At least **1 approving reviewer** (other than the PR author).
   * All CI gates green (lint, test-py, test-cpp, sanitize, clang-tidy,
     coverage-cpp, docs).
   * No open S1 PRACA issues.

2. **S1 safety fixes** also require:

   * At least **2 approving reviewers**, one of whom must be the Lead Developer.
   * SWIFI suite 100% pass and the CI coverage gate green (design target
     100% MC/DC on safety-tagged decision points; 30% whole-library
     CI-enforced floor — see ``assurance/metrics_plan.rst``) on the fix
     branch before merge is attempted.

3. **Baseline establishment** (appending a row to this register) requires:

   * Verification phase exit criteria fully met (per ``sdp.rst``).
   * Golden test vectors committed to ``test-vectors/``.
   * Software Version Description populated and committed for the tag
     (the SVD, ``svd.rst``, is maintained in the development tree and is
     not shipped in this release).
   * Lead Developer approval on the baseline PR.

4. **Emergency hot-fixes** (S1 defect discovered post-baseline):

   * A new patch-level tag is created (e.g., ``v0.2.1``).
   * The hot-fix must satisfy all S1 fix-verification requirements of the
     PRACA process (the PRACA record, ``praca.rst``, is maintained in the
     development tree and is not shipped in this release).
   * A new BL row is appended to this register for the patch tag.
   * The SVD for the affected baseline is superseded by the patch SVD.
