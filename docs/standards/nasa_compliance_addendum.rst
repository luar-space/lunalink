NASA Compliance Addendum
========================

**Document ID**: LUNALINK-NASA-ADD-001
**Specification Reference**: NPR 7150.2D (secondary cross-reference)
**Revision**: 1.0
**Date**: 2026-03-13
**Status**: Released — Competition Submission Baseline

Purpose
-------

LunaLink's assurance infrastructure is natively structured against **ECSS-E-ST-40C Rev.1**
and **ECSS-Q-ST-80C Rev.2**.  This addendum provides a single lookup table mapping all 29
NASA NPR 7150.2D SWE requirements to their ECSS equivalents and to the LunaLink artefacts
that satisfy them.

For the technical software engineering requirements relevant to LunaLink's scope, ECSS
imposes requirements at least as stringent as the corresponding NPR SWE requirements.
This addendum enables evaluators familiar with the NASA framework to locate compliance
evidence without navigating the ECSS structure directly.

For the ECSS tailoring decisions themselves, see ``docs/standards/esa/ecss_standards.rst``
(LUNALINK-ECSS-TAIL-001).

Compliance Matrix
-----------------

.. list-table::
   :header-rows: 1
   :widths: 10 28 15 32 15

   * - SWE ID
     - SWE Requirement
     - ECSS Clause
     - LunaLink Artefact
     - Status
   * - SWE-013
     - Software Development Plan
     - E-ST-40C §5.2
     - ``governance/sdp.rst``
     - Satisfied
   * - SWE-020
     - Formal Milestone Reviews (SRR/PDR/CDR)
     - E-ST-40C §5.1
     - Gateway model in ``governance/baseline_register.rst``
     - Organisational
   * - SWE-027
     - Software Safety Plan
     - Q-ST-80C §5.3/§5.5
     - ``assurance/fmea.rst``, ``assurance/fault_coverage.rst``
     - Satisfied
   * - SWE-028
     - Software FMEA — failure mode enumeration
     - Q-ST-80C §5.3/§5.5
     - ``assurance/fmea.rst``
     - Satisfied
   * - SWE-029
     - Software FMEA — detection & containment
     - Q-ST-80C §5.3/§5.5
     - ``assurance/fmea.rst``
     - Satisfied
   * - SWE-042
     - Software Architecture
     - E-ST-40C §5.4.2
     - ``signal/architecture.rst``
     - Satisfied
   * - SWE-050
     - COTS/OSS Assessment
     - Q-ST-80C §5.7
     - ``assurance/tool_confidence.rst``
     - Satisfied
   * - SWE-058
     - Detailed Design
     - E-ST-40C §5.4.3
     - ``signal/algorithms.rst``, ``signal/bch_decoder_design.rst``
     - Satisfied
   * - SWE-070
     - OSS Version Control & Licence
     - Q-ST-80C §5.7
     - ``assurance/tool_confidence.rst``
     - Satisfied
   * - SWE-074
     - Configuration Management (technical)
     - Q-ST-80C §5.4
     - ``governance/scmp.rst``
     - Satisfied
   * - SWE-077
     - Formal Baselines
     - Q-ST-80C §5.4
     - ``governance/baseline_register.rst``
     - Satisfied
   * - SWE-079
     - Change Control Board (CCB)
     - Q-ST-80C §5.4
     - GitHub PR review (proxy); formal CCB deferred
     - Organisational
   * - SWE-083
     - Software Requirements Specification
     - E-ST-40C §5.3
     - ``signal/shall_inventory.rst``
     - Satisfied
   * - SWE-088
     - Requirements Traceability
     - E-ST-40C §5.3
     - ``signal/compliance_matrix.rst``
     - Satisfied
   * - SWE-090
     - Interface Control Document
     - E-ST-40C §5.4
     - ``signal/icd.rst``
     - Satisfied
   * - SWE-104
     - Software Architecture Document
     - E-ST-40C §5.4.2
     - ``signal/architecture.rst``, ``signal/signal_chain.rst``
     - Satisfied
   * - SWE-107
     - Coding Standards
     - E-ST-40C §5.5.1
     - ``mission_assurance.rst`` §Design Constraints
     - Satisfied
   * - SWE-108
     - Static Analysis
     - Q-ST-80C §5.5
     - ``static_analysis_report.rst``, ``cppcheck_report.rst``
     - Satisfied
   * - SWE-109
     - Forbidden Language Constructs
     - E-ST-40C §5.5.1
     - Compiler flags; ``scripts/check_build_flags.py``
     - Satisfied
   * - SWE-117
     - Code Review / Inspection
     - Q-ST-80C §5.5
     - GitHub PR review (mandatory approval gate); supplemented by Clang-Tidy +
       Cppcheck automated inspection
     - Satisfied
   * - SWE-121
     - Independent V&V (IV&V)
     - Q-ST-80C §5.5
     - Deferred to flight programme
     - Organisational
   * - SWE-127
     - Unit Testing (MC/DC)
     - E-ST-40C §5.5.3
     - CI MC/DC report; 100% on tagged functions
     - Satisfied
   * - SWE-128
     - Integration Testing
     - E-ST-40C §5.5.4
     - Digital Twin; Golden Test Vector Suite
     - Satisfied
   * - SWE-134
     - Integration Testing (system)
     - E-ST-40C §5.5.4
     - Python/C++ integration tests
     - Satisfied
   * - SWE-139
     - Acceptance Testing
     - E-ST-40C §5.6
     - CI-based recertification
     - Satisfied
   * - SWE-155
     - Physical Configuration Audit (PCA)
     - Q-ST-80C §5.4
     - Reproducible build capability; audit deferred
     - Organisational
   * - SWE-164
     - Software Metrics Plan
     - Q-HB-80-04A
     - ``assurance/metrics_plan.rst``
     - Satisfied
   * - SWE-168
     - PRACA
     - Q-ST-80C §5.6
     - ``assurance/praca.rst``
     - Satisfied
   * - SWE-181
     - Software Version Description
     - E-ST-40C §5.6
     - ``assurance/svd.rst``
     - Satisfied

Organisational Scope Note
-------------------------

Four SWE requirements (SWE-020, SWE-079, SWE-121, SWE-155) require programme office
infrastructure — independent review teams, a formal Change Control Board, IV&V
contractors, and physical configuration audits.  These are structurally outside the
scope of a development team acting alone and are documented in full in
the SDP (``docs/governance/sdp.rst``) and the ECSS tailoring record
(``docs/standards/esa/ecss_standards.rst``).

The technical artefacts required by these SWEs are in place and structured for
immediate consumption by a programme office upon flight adoption.
