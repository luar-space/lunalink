ECSS Software Certification Standards
======================================

:Document ID: LUNALINK-STDS-001
:Last Updated: 2026-03-13

A catalogue of the European Cooperation for Space Standardization (ECSS)
standards that bear on space software certification.  LunaLink's
assurance infrastructure is natively structured against **ECSS-E-ST-40C Rev.1**
and **ECSS-Q-ST-80C Rev.2**, with NASA NPR 7150.2D maintained as a secondary
cross-reference (see ``docs/standards/nasa_compliance_addendum.rst``).

.. contents:: On this page
   :local:
   :depth: 2


Standards Overview
------------------

ECSS software standardisation spans two branches:

* **Engineering (E-40 family)** — software development lifecycle, reviews,
  design, coding, V&V.
* **Product Assurance (Q-80 family)** — software criticality categories,
  independent V&V, configuration management, problem reporting.


Primary Standards
-----------------

ECSS-E-ST-40C Rev.1 — Software
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``ECSS-E-ST-40C-Rev.1(30April2025).pdf``
:Date: 30 April 2025
:Supersedes: ECSS-E-ST-40C (6 March 2009)
:Scope: All product software developed as part of space system projects —
        space segment, launch services, and ground segment.  Covers
        requirements definition, design, production, verification and
        validation, transfer, operations, and maintenance.
:Based on: ISO/IEC 12207

This is the **primary software engineering standard** for ESA projects.
It defines:

* Review milestones (SRR, PDR, CDR, QR, AR) — analogous to NASA's review gates.
* Software development lifecycle phases and activities.
* Requirements engineering, architectural and detailed design.
* Coding standards and unit/integration/validation testing.
* Configuration management and delivery.

**Key sections for LunaLink:**

.. list-table::
   :header-rows: 1
   :widths: 20 50 30

   * - Section
     - Topic
     - NPR 7150.2D Equivalent
   * - §5.2
     - Software Development Plan
     - SWE-013
   * - §5.3
     - Software Requirements
     - SWE-083
   * - §5.4
     - Software Design
     - SWE-042 / SWE-058
   * - §5.5
     - Software Implementation & Testing
     - SWE-107 / SWE-127 / SWE-134
   * - §5.6
     - Software Delivery & Acceptance
     - SWE-139
   * - §5.7
     - Software Maintenance
     - SWE-168 (PRACA)


ECSS-Q-ST-80C Rev.2 — Software Product Assurance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``ECSS-Q-ST-80C-Rev.2(30April2025).pdf``
:Date: 30 April 2025
:Supersedes: ECSS-Q-ST-80C Rev.1 (15 February 2017)
:Scope: Software product assurance requirements for developing and
        maintaining software in space systems — spacecraft, launchers,
        payloads, and ground equipment.

This is the **quality and assurance** companion to E-ST-40C.  It defines:

* Software criticality categories (A through D).
* Independent verification and validation (IV&V) requirements.
* Configuration management and baseline control.
* Problem reporting and corrective action (PRACA).
* Software reuse assessment.

**Key sections for LunaLink:**

.. list-table::
   :header-rows: 1
   :widths: 20 50 30

   * - Section
     - Topic
     - NPR 7150.2D Equivalent
   * - §5.2
     - Software Classification (Criticality)
     - Class A–D (NPR 7150.2 §3.1)
   * - §5.3
     - Software Product Assurance Planning
     - SWE-013 (SDP assurance sections)
   * - §5.4
     - Configuration Management
     - SWE-077
   * - §5.5
     - Verification & Validation
     - SWE-127 / SWE-134
   * - §5.6
     - Problem Reporting (PRACA)
     - SWE-168
   * - §5.7
     - Software Reuse
     - (OSS assessment)


Supporting Handbooks
--------------------

ECSS-E-HB-40A — Software Engineering Handbook
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``ECSS-E-HB-40A(11December2013).pdf``
:Date: 11 December 2013
:Purpose: Advice, interpretations, and best practices for implementing
          ECSS-E-ST-40C.  Covers both flight and ground software.

Provides worked examples of:

* Requirements specification templates.
* Design description document structure.
* Test plan and procedure formats.
* Review data package contents.


ECSS-E-HB-40-01A — Agile Software Development Handbook
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``ECSS-E-HB-40-01A(7April2020).pdf``
:Date: 7 April 2020
:Purpose: Guidance on applying agile development methods within the ECSS
          framework.  Addresses how iterative delivery maps to ECSS review
          milestones and how to maintain assurance evidence in an agile
          workflow.


ECSS-Q-HB-80-03A Rev.1 — Software Dependability and Safety
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``ECSS-Q-HB-80-03A-Rev.1(20November2017).pdf``
:Date: 20 November 2017
:Purpose: Guidance on software dependability analysis, safety assessment,
          and FMEA/FMECA for space software.

Directly relevant to LunaLink's:

* ``docs/assurance/fmea.rst`` — Failure Mode and Effects Analysis.
* ``docs/assurance/fmea.rst`` + ``docs/assurance/fault_coverage.rst`` — software safety analysis and fault-coverage evidence.
* ``docs/assurance/fault_coverage.rst`` — SWIFI fault coverage matrix.


ECSS-Q-HB-80-04A — Software Metrication Handbook
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``ECSS-Q-HB-80-04A(30March2011).pdf``
:Date: 30 March 2011
:Purpose: Guidance on defining and implementing a software metrication
          programme — goal specification, metric selection, data collection,
          analysis, and continuous improvement.

Relevant to LunaLink's ``docs/assurance/metrics_plan.rst``.


NPR 7150.2D to ECSS Cross-Reference
------------------------------------

For the complete NPR 7150.2D SWE-to-ECSS clause mapping, see the NASA Compliance
Addendum (LUNALINK-NASA-ADD-001, ``docs/standards/nasa_compliance_addendum.rst``).


LunaLink Criticality Classification
------------------------------------

Under ECSS-Q-ST-80C, LunaLink is classified as:

* **Category B** (mission-critical) — failure of the AFS signal-processing
  chain would result in loss of navigation service to the receiving
  spacecraft, but would not directly cause loss of life or vehicle.

Category B requires:

* Full requirements traceability.
* Unit, integration, and validation testing.
* Independent review of safety-critical functions.
* Configuration management with formal baselines.
* FMEA and software safety analysis.

LunaLink's current assurance infrastructure satisfies or exceeds these
requirements on the technical side; the organisational requirements are
tailored for the solo-developer competition context as recorded below.


Tailoring Record
----------------

LunaLink is developed by a single engineer for the competition submission.
The following organisational requirements cannot be satisfied as written by a
development team acting alone; each is tailored honestly rather than claimed.
The technical artefacts each process would consume are in place and are
structured for hand-over to a programme office at flight adoption.

.. list-table::
   :header-rows: 1
   :widths: 18 16 30 36

   * - Requirement Area
     - ECSS Clause
     - Category B Expectation
     - LunaLink Tailoring
   * - Independent Verification & Validation (IV&V)
     - Q-ST-80C §5.5
     - Verification activities performed by a party independent of the
       development team
     - **Tailored: self-performed.**  Verification is executed by the
       developer via automated CI gates (coverage, SWIFI, static analysis,
       sanitizers) rather than an independent party.  Q-ST-80C §5.5
       independence is noted as unmet; the evidence artefacts are structured
       for independent re-execution (single-command reproduction).
   * - Formal reviews (SRR/PDR/CDR/QR/AR)
     - E-ST-40C §5.1
     - Milestone reviews with an independent review board
     - **Tailored: solo-developer gate reviews.**  The competition gateway
       sequence substitutes for the E-ST-40C §5.1 milestone reviews: each
       gateway exit is a self-conducted review against documented exit
       criteria (``governance/sdp.rst``), recorded in
       ``governance/baseline_register.rst``.  No independent board exists.
   * - Change Control Board (CCB)
     - Q-ST-80C §5.4
     - Multi-party board approving changes to formal baselines
     - **Tailored: single maintainer + PR discipline.**  All changes flow
       through pull requests with full CI gates; baseline changes are
       append-only rows in the baseline register.  The single maintainer is
       the sole change authority.
   * - Configuration audits (PCA / FCA)
     - Q-ST-80C §5.4
     - Physical and functional configuration audits by an audit authority
     - **Tailored: automated evidence generation.**  FCA evidence is the
       golden-vector and SWIFI suites in CI; PCA evidence is the
       reproducible-build procedure and ``evidence/build_manifest.json``.
       No independent auditor has executed either audit.


Document Inventory
------------------

The referenced PDFs are not shipped in this repository (licensing); they are
obtainable by running ``bash docs/standards/download_standards.sh``, which
fetches them into ``docs/standards/esa/``:

.. list-table::
   :header-rows: 1
   :widths: 55 15 15 15

   * - Document
     - Type
     - Date
     - Size
   * - ECSS-E-ST-40C-Rev.1(30April2025).pdf
     - Standard
     - 2025-04-30
     - 2.0 MB
   * - ECSS-Q-ST-80C-Rev.2(30April2025).pdf
     - Standard
     - 2025-04-30
     - 1.3 MB
   * - ECSS-E-HB-40A(11December2013).pdf
     - Handbook
     - 2013-12-11
     - 1.9 MB
   * - ECSS-E-HB-40-01A(7April2020).pdf
     - Handbook
     - 2020-04-07
     - 1.6 MB
   * - ECSS-Q-HB-80-03A-Rev.1(20November2017).pdf
     - Handbook
     - 2017-11-20
     - 464 KB
   * - ECSS-Q-HB-80-04A(30March2011).pdf
     - Handbook
     - 2011-03-30
     - 1.4 MB


Sources
-------

All documents obtained from the official ECSS website:
`European Cooperation for Space Standardization <https://ecss.nl>`_.
