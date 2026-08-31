ECSS Software Certification Standards
======================================

:Document ID: LUNALINK-STDS-001
:Last Updated: 2026-03-13

This document catalogues the European Cooperation for Space Standardization
(ECSS) standards relevant to space software certification.  LunaLink's
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
requirements.  See the per-requirement applicability columns in this document for the full tailoring
record.


Document Inventory
------------------

All referenced PDFs are stored in ``docs/standards/esa/``:

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
