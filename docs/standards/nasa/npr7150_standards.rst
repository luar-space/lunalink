NASA NPR 7150.2D Software Engineering Standards
=================================================

:Document ID: LUNALINK-STDS-002
:Last Updated: 2026-03-13

A catalogue of the NASA software engineering standards that bear on space
software certification.  LunaLink's assurance infrastructure is built
against **NPR 7150.2D** (Class B equivalent).

.. contents:: On this page
   :local:
   :depth: 2


Standards Overview
------------------

NASA's software engineering governance is a hierarchy:

1. **NPR 7150.2D** — the procedural requirement (the "shall" statements).
2. **NASA-STD-8739.8B** — software assurance and safety standard
   (IV&V, safety-critical requirements, hazard analysis).
3. **NASA-HDBK-2203** — the software engineering handbook
   (guidance, best practices, worked examples).
4. **NASA-STD-7009B** — standard for models and simulations
   (credibility assessment when M&S is used for V&V).

All SWE-numbered requirements originate from NPR 7150.2D.


Primary Standard
-----------------

NPR 7150.2D — NASA Software Engineering Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``NPR-7150.2D.pdf``
:Effective: 8 March 2022
:Expires: 8 March 2027
:Responsible Office: Office of the Chief Engineer
:Applicability: All NASA-wide software — flight, ground, and embedded
:Classification: Mandatory for NASA employees; flowed to contractors via SOW

This is the **governing procedural requirement** for all NASA software.  It
defines ~180 SWE-numbered requirements organised into three layers:

* **Chapter 3** — Software Management Requirements (planning, classification,
  assurance, safety-critical, reuse, cybersecurity, traceability).
* **Chapter 4** — Software Engineering Life Cycle Requirements (requirements,
  architecture, design, implementation, testing, operations/maintenance).
* **Chapter 5** — Supporting Life Cycle Requirements (configuration management,
  risk management, peer reviews, measurements, defect management).

Software Classification
~~~~~~~~~~~~~~~~~~~~~~~~

NPR 7150.2D classifies software into six classes (Appendix D).
Requirements are invoked per class via the Requirements Mapping Matrix
(Appendix C):

.. list-table::
   :header-rows: 1
   :widths: 12 58 30

   * - Class
     - Description
     - Example
   * - **A**
     - Human-Rated Space Software Systems
     - ISS flight software
   * - **B**
     - Non-Human Space-Rated Software Systems or Large-Scale Aeronautics
     - LunaLink AFS (navigation service)
   * - **C**
     - Mission Support Software or Aeronautic Vehicles, or Major
       Engineering/Research Facility Software
     - Ground station tools
   * - **D**
     - Basic Science/Engineering Design and Research and Technology Software
     - Analysis scripts
   * - **E**
     - Design Concept, Research, Technology and General Purpose Software
     - Prototypes
   * - **F**
     - General Purpose Computing, Business and IT Software
     - Administrative tools


Chapter 3 — Software Management Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 12 48 40

   * - SWE #
     - Requirement (summary)
     - LunaLink Evidence
   * - SWE-013
     - Develop, maintain, and execute software plans covering the entire
       life cycle
     - ``governance/sdp.rst``
   * - SWE-021
     - Update plans when software classification changes
     - ``governance/baseline_register.rst``
   * - SWE-032
     - Develop with CMMI-DEV rated organisation
     - N/A (tailored — open-source project)
   * - SWE-033
     - Assess acquisition vs. development options
     - ``assurance/tool_confidence.rst``
   * - SWE-052
     - Bi-directional traceability
     - ``signal/compliance_matrix.rst``
   * - SWE-156
     - Cybersecurity assessment on software components
     - ``assurance/tool_confidence.rst`` (dependency audit)
   * - SWE-207
     - Identify and implement secure coding practices
     - HICPP clang-tidy, ``-fno-exceptions -fno-rtti``

Chapter 4 — Software Engineering Life Cycle Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 12 48 40

   * - SWE #
     - Requirement (summary)
     - LunaLink Evidence
   * - SWE-050
     - Establish, capture, and maintain software requirements
     - ``signal/shall_inventory.rst`` (requirement inventory)
   * - SWE-051
     - Perform requirements analysis from system-level requirements
     - ``signal/compliance_matrix.rst``
   * - SWE-053
     - Track and manage changes to requirements
     - Git + ``governance/scmp.rst``
   * - SWE-055
     - Perform requirements validation
     - ``signal/validation_plan.rst``
   * - SWE-057
     - Transform requirements into a recorded software architecture
     - ``signal/architecture.rst``
   * - SWE-058
     - Develop and maintain a software design
     - ``signal/signal_chain.rst``, ``signal/algorithms.rst``
   * - SWE-060
     - Implement the software design into code
     - core C++ source directories
   * - SWE-061
     - Select and adhere to coding methods, standards, and criteria
     - ``.clang-tidy``, ``pyproject.toml [tool.ruff]``
   * - SWE-062
     - Unit test the software code
     - ``cpp/tests/``, ``tests/``, ``task test``
   * - SWE-063
     - Provide a software version description for each release
     - ``svd.rst`` (maintained in the development tree; not shipped in
       this release)
   * - SWE-065
     - Establish and maintain test plans, procedures, tests, and reports
     - ``signal/validation_plan.rst``, CI workflows
   * - SWE-066
     - Test the software against its requirements
     - Catch2 + pytest suites
   * - SWE-068
     - Evaluate test results and record the evaluation
     - CI artifact uploads, ``recertification_record.rst`` (generated on
       demand by ``task recertify``)
   * - SWE-070
     - Use validated models/simulations for qualification of flight software
     - ``wcet_profile.rst``, ``[performance]`` benchmarks
   * - SWE-071
     - Update test/verification plans to be consistent with requirements
     - Updated each gateway
   * - SWE-073
     - Validate the software on the targeted platform or high-fidelity
       simulation
     - Scoped to flight integration (digital twin only)
   * - SWE-075
     - Plan and implement software operations, maintenance, and retirement
     - ``governance/sdp.rst`` §maintenance
   * - SWE-077
     - Complete and deliver the software with appropriate records
     - ``governance/baseline_register.rst``
   * - SWE-135
     - Use static analysis tools (defects, security, coverage, complexity)
     - clang-tidy, cppcheck, pyright, ruff, radon
   * - SWE-186
     - Assure unit test results are repeatable
     - Deterministic test suite, CI matrix
   * - SWE-189
     - Select, implement, track, record, and report code coverage
     - ``coverage-cpp.yml`` (LLVM MC/DC)
   * - SWE-190
     - Verify code coverage by analysis of test execution results
     - LLVM ``llvm-cov report`` in CI
   * - SWE-191
     - Regression testing to demonstrate no new defects introduced
     - Full test suite on every push + pre-commit hook
   * - SWE-192
     - Verify through test the requirements tracing to hazardous events
     - SWIFI suite — 34 scenarios, 42 test cases; see
       ``assurance/fault_coverage.rst``
   * - SWE-194
     - Verify all requirements met prior to delivery
     - ``task recertify`` / ``recertify.yml``

Chapter 5 — Supporting Life Cycle Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 12 48 40

   * - SWE #
     - Requirement (summary)
     - LunaLink Evidence
   * - SWE-079
     - Develop a software configuration management plan
     - ``governance/scmp.rst``
   * - SWE-080
     - Track and evaluate changes to software products
     - Git history, PR reviews
   * - SWE-081
     - Identify software configuration items and versions
     - ``governance/baseline_register.rst``
   * - SWE-082
     - Establish change control procedures
     - ``governance/scmp.rst`` §change control
   * - SWE-083
     - Prepare and maintain records of configuration status
     - ``svd.rst`` (maintained in the development tree; not shipped in
       this release)
   * - SWE-084
     - Perform software configuration audits
     - ``task ci-checks``
   * - SWE-086
     - Record, analyse, plan, track, control software risks
     - ``praca.rst`` (maintained in the development tree; not shipped in
       this release)
   * - SWE-087
     - Perform and report peer reviews/inspections
     - PR review process, ``governance/sdp.rst``


Companion Standards
-------------------

NASA-STD-8739.8B — Software Assurance and Software Safety
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``NASA-STD-8739.8B.pdf``
:Date: 8 September 2022
:Status: Active (mandatory)
:Purpose: Defines a systematic approach to software assurance, software
          safety, and Independent Verification & Validation (IV&V).

This standard is the **assurance and safety** counterpart to NPR 7150.2D.
While NPR 7150.2D tells the project manager *what* to do, NASA-STD-8739.8B
tells the assurance team *how to verify* it was done correctly.

Key topics:

* Software assurance planning and process monitoring.
* Software safety analysis (hazard analysis, FMEA, fault tree).
* Independent Verification & Validation (IV&V) criteria.
* Metrics collection and reporting for assurance.
* Software safety-critical determination and classification.

**Relevance to LunaLink:**

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - 8739.8B Topic
     - LunaLink Evidence
   * - Software safety analysis
     - ``assurance/fmea.rst``, ``assurance/fault_coverage.rst``
   * - Fault injection / fault tolerance
     - ``assurance/fault_coverage.rst``, SWIFI suite
   * - Assurance planning
     - ``assurance/mission_assurance.rst``
   * - Problem reporting (PRACA)
     - PRACA process (``praca.rst``, maintained in the development tree;
       not shipped in this release); ``.github/ISSUE_TEMPLATE/bug_report.yml``
   * - Metrics
     - ``assurance/metrics_plan.rst``


NASA-STD-7009B — Standard for Models and Simulations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``NASA-STD-7009B.pdf``
:Date: 5 March 2024
:Status: Active
:Purpose: Establishes requirements for development, acceptance, and use of
          models and simulations (M&S) in NASA activities.

Relevant when M&S is used for V&V (e.g., BER simulation to validate LDPC
decoder performance, WCET profiling as evidence for timing requirements).

Key concepts:

* **Credibility assessment** — 8 factors: verification, validation,
  input pedigree, results uncertainty, use history, M&S management,
  people qualifications, and limitations.
* **Intended use statement** — each M&S must declare its intended use
  and any limitations.

**Relevance to LunaLink:**

* BER performance simulations (``[performance]`` Catch2 tests).
* WCET profiling (``wcet.yml``, ``wcet_profile.rst``).
* Digital twin as a high-fidelity simulation of the flight signal chain.


NASA-HDBK-7009A — Handbook for Models and Simulations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Document: ``NASA-HDBK-7009A.pdf``
:Date: 2014
:Purpose: Implementation guide for NASA-STD-7009.  Provides worked examples
          of credibility assessments, M&S management plans, and intended use
          statements.


NASA-HDBK-2203 — Software Engineering Handbook
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:Access: `swehb.nasa.gov <https://swehb.nasa.gov>`_ (wiki-based; no standalone PDF)
:Version: B (active)
:Purpose: Best-practices guidance for implementing NPR 7150.2D and
          NASA-STD-8739.8B requirements.

.. note::

   NASA-HDBK-2203 is published as a Confluence wiki, not a downloadable PDF.
   It is the authoritative interpretation guide for every SWE requirement.
   The 1-page PDF cover sheet on ``standards.nasa.gov`` simply redirects
   to the wiki.


LunaLink Compliance Summary
----------------------------

LunaLink targets **Class B** (non-human space-rated software systems).
The current assurance infrastructure addresses the following NPR 7150.2D
chapters:

.. list-table::
   :header-rows: 1
   :widths: 40 20 40

   * - Area
     - Status
     - Key Artefacts
   * - Software Development Plan (SWE-013)
     - Complete
     - ``governance/sdp.rst``
   * - Software Requirements (SWE-050–055)
     - Complete
     - ``signal/shall_inventory.rst``, ``signal/compliance_matrix.rst``
   * - Software Architecture (SWE-057)
     - Complete
     - ``signal/architecture.rst``
   * - Software Design (SWE-058)
     - Complete
     - ``signal/signal_chain.rst``, ``signal/algorithms.rst``
   * - Coding Standards (SWE-061)
     - Complete
     - ``.clang-tidy``, ``pyproject.toml``
   * - Unit Testing (SWE-062)
     - Complete
     - 302 Catch2 test cases (including the 42-case SWIFI suite), pytest suite
   * - Static Analysis (SWE-135)
     - Complete
     - clang-tidy, cppcheck, pyright, ruff
   * - Code Coverage (SWE-189/190)
     - Complete
     - LLVM line + MC/DC coverage
   * - Configuration Management (SWE-079–085)
     - Complete
     - ``governance/scmp.rst``, ``governance/baseline_register.rst``
   * - Problem Reporting (SWE-086/087)
     - Complete
     - ``praca.rst`` (maintained in the development tree; not shipped in
       this release)
   * - Software Version Description (SWE-063)
     - Complete
     - ``svd.rst`` (maintained in the development tree; not shipped in
       this release)
   * - Fault Injection Testing (SWE-192)
     - Complete
     - 42 SWIFI test cases covering 34 scenarios (see
       ``assurance/fault_coverage.rst``)
   * - Recertification (SWE-194)
     - Complete
     - ``recertify.yml``, ``task recertify``
   * - Safety Analysis
     - Complete
     - ``assurance/fmea.rst``, ``assurance/fault_coverage.rst``
   * - Validation on Target Platform (SWE-073)
     - Scoped to Flight
     - Digital twin only; flight target at programme adoption
   * - Cybersecurity (SWE-156/207)
     - Partial
     - OSS assessment done; formal threat model at Gateway 4


Document Inventory
------------------

The referenced PDFs are not shipped in this repository (licensing); they are
obtainable by running ``bash docs/standards/download_standards.sh``, which
fetches them into ``docs/standards/nasa/``:

.. list-table::
   :header-rows: 1
   :widths: 50 18 14 18

   * - Document
     - Type
     - Date
     - Size
   * - NPR-7150.2D.pdf
     - Procedural Req.
     - 2022-03-08
     - 553 KB
   * - NASA-STD-8739.8B.pdf
     - Standard
     - 2022-09-08
     - 676 KB
   * - NASA-STD-7009B.pdf
     - Standard
     - 2024-03-05
     - 1.4 MB
   * - NASA-HDBK-7009A.pdf
     - Handbook
     - 2014
     - 2.6 MB


Sources
-------

* `NPR 7150.2D — NODIS Library <https://nodis3.gsfc.nasa.gov/displayDir.cfm?t=NPR&c=7150&s=2D>`_
* `NASA-STD-8739.8B — NASA Standards <https://standards.nasa.gov/standard/NASA/NASA-STD-87398>`_
* `NASA-STD-7009B — NASA Standards <https://standards.nasa.gov/standard/NASA/NASA-STD-7009>`_
* `NASA-HDBK-2203 — Software Engineering Handbook (wiki) <https://swehb.nasa.gov>`_
* `NASA Software Engineering Resources <https://www.nasa.gov/intelligent-systems-division/software-management-office/nasa-software-engineering-procedural-requirements-standards-and-related-resources/>`_
