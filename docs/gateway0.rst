.. meta::
   :description: Phase 1 deliverable for the ESA-CCSDS Protocol Development Competition

=========================================================================
LSIS-AFS Reference Implementation — Standards Compliance & Architecture
=========================================================================

.. only:: latex

   .. raw:: latex

      \begin{center}
      \large Phase 1 Deliverable — ESA-CCSDS Protocol Development Competition \\[0.5em]
      \normalsize Milestone 1, 31 March 2026 \\[2em]
      \end{center}

.. list-table:: Document Control
   :widths: 25 75

   * - **Document ID**
     - LUNALINK-GW0-001
   * - **Version**
     - 1.0
   * - **Date**
     - 31 March 2026
   * - **Classification**
     - Competition Deliverable
   * - **Licence**
     - Apache-2.0 (SPDX: ``Apache-2.0``).  Full licence text in ``LICENSE`` at repository root.

----

Executive Summary
-----------------

LunaLink is a dual-layer C++20/Python reference implementation of the LSIS-AFS
specification (V1.0, 29 January 2025).  This document presents the Phase 1
deliverable for the ESA-CCSDS Protocol Development Competition: standards
compliance analysis, system architecture, module interfaces, testing strategy,
development environment, and reference data validation.

During specification study, three specification gaps in LSIS V1.0 were identified and
formally documented with proposed resolutions.  The system architecture has
been designed around a deterministic C++ core with no exceptions, no RTTI, and
no dynamic allocation on the signal path, wrapped by Python bindings for
prototyping and test-vector generation.  Both specification data annexes
(Annex 2: LDPC submatrices, Annex 3: PRN spreading codes) have been loaded,
converted to native data structures, and validated against the reference
material.

----

.. _gd01:

Specification Analysis
----------------------

LunaLink identified three specification gaps in LSIS V1.0 that require
implementation assumptions for deterministic interoperability.  Each finding
documents the affected section, the interoperability risk, and LunaLink's
adopted resolution.

.. list-table:: LSIS V1.0 Specification Findings
   :header-rows: 1
   :widths: 5 45 50

   * - #
     - Finding
     - Adopted Resolution
   * - 1
     - **Matched-code phasing TBD** (|S|\ 2.3.5.4, LSIS-TBD-2001).
       Code-to-node assignments and tertiary code phasing offsets are explicitly
       deferred; independent receivers cannot achieve pilot lock without
       agreed phasing.
     - For integration and test, use the interim Table 11 assignments and set
       tertiary phase offset to 0 for PRN 1--12, but treat this as a
       configurable parameter (not hardcoded).  For future releases, place the
       phase offsets in the MCID table so receivers can load phasing from
       configuration without code changes; define the default as 0 when an
       entry is absent.
   * - 2
     - **Navigation message types incomplete** (|S|\ 2.5, Table 21).
       Table 21 defines 23 message types; 14 are designated LNIS V1.0.
       Of those 14, only MSG-G8 (Time of Transmission) includes complete
       bit-level field definitions — the remaining 13 are marked TBW.
       Full 12 s frame construction is blocked without bit-level layouts
       for these types.
     - LunaLink provides draft bit-layouts via a SISICD extension document
       (``docs/signal/sisicd.rst``; e.g., G3 High-Precision Clock).  These
       enable end-to-end frame validation and will be retired once the
       formal SISICD is published.
   * - 3
     - **No baseband digital interchange format**.
       LSIS V1.0 is a signal-in-space specification and does not define a
       standardised digital baseband format for SDR-to-SDR exchange.
       Without one, teams cannot cross-validate transmitter and receiver
       implementations at the IQ level.
     - LunaLink proposes interleaved signed 16-bit IQ pairs at the AFS-Q
       chip rate (5.115 MSPS) as a digital interchange format for
       hardware-in-the-loop and cross-team validation.  This is a LunaLink
       proposal, not an LSIS or competition requirement.

.. |S| unicode:: U+00A7

A full findings report with rationale and standard references is maintained
in the project documentation.

----

.. _gd02:

System Architecture & Code Structure
-------------------------------------

Design Philosophy
~~~~~~~~~~~~~~~~~

The system is a dual-layer design:

- **Core layer (C++20):** Deterministic signal processing compiled with
  ``-fno-exceptions -fno-rtti -fno-fast-math``.  All signal-path functions
  use ``std::span`` for memory safety without dynamic allocation.  Critical
  paths use bitwise logic and C++20 features (``constinit``, ``std::popcount``).

- **Wrapper layer (Python 3.12+):** nanobind bindings for prototyping,
  test-vector generation, and analysis.

This separation ensures the core can be extracted for embedded or
flight-software contexts while the Python layer provides accessibility for
researchers and integrators.

Module Design
~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 15 75

   * - Module
     - Gateway
     - Responsibility
   * - ``prn``
     - G1
     - Spreading Code Loader (Gold-2046, Weil-10230, Weil-1500)
   * - ``matched_code``
     - G1
     - Tiered Code Combiner (primary XOR secondary XOR tertiary)
   * - ``bch``
     - G2
     - BCH(51,8) Encoder and ML Correlation Decoder
   * - ``ldpc``
     - G2
     - LDPC 5G NR Encoder (SF2/SF3/SF4) and Layered Sum-Product Decoder
   * - ``interleaver``
     - G2
     - Block Interleaver/Deinterleaver (60\ |x|\ 98)
   * - ``crc``
     - G2
     - CRC-24Q Generator and Validator
   * - ``frame``
     - G3
     - Subframe 1--4 Builders and Frame Assembler
   * - ``modulator``
     - G4
     - BPSK Modulators (AFS-I at 1.023 Mcps, AFS-Q at 5.115 Mcps)
   * - ``iq_mux``
     - G4
     - IQ Multiplexer (5.115 MSPS output)
   * - ``sync``
     - G5
     - Frame Synchronisation and Symbol Extraction
   * - ``parser``
     - G6
     - Subframe Parsers and Time-of-Transmission Calculator

.. |x| unicode:: U+00D7

Signal Chain
~~~~~~~~~~~~

The transmitter-side signal chain processes navigation data from message bits
through FEC encoding, interleaving, spreading, modulation, and IQ
multiplexing.  The two pipelines below show the message encoding path
(navigation data to encoded frame) and the spreading/modulation path
(encoded frame to baseband I/Q output).

.. only:: latex

   .. raw:: latex

      \begin{figure}[ht]
      \centering
      \begin{tikzpicture}[
        block/.style={draw, rounded corners=2pt, minimum width=36mm,
                      minimum height=10mm, align=center, font=\small},
        op/.style={draw, rounded corners=2pt, minimum width=30mm,
                   minimum height=9mm, align=center, font=\small\itshape,
                   fill=black!5},
        arr/.style={-{Stealth[length=2.5mm]}, thick},
        every node/.style={font=\small},
      ]
      % --- Message Encoding Pipeline (two-column layout) ---
      % Left column: Subframe 1 path
      \node[block] (sb1)  at (-2.2, 0)    {Subframe~1\\FID + TOI};
      \node[op]    (bch)  at (-2.2,-1.4)  {BCH(51,8)\\9\,b $\to$ 52\,sym};

      % Right column: Subframes 2-4 path
      \node[block] (sb24) at (2.2, 0)     {Subframes~2\textendash4\\CED, Almanac, Access};
      \node[op]    (crc)  at (2.2,-1.4)   {CRC-24Q\\+\,24\,b};
      \node[op]    (ldpc) at (2.2,-2.7)   {LDPC(1/2)};
      \node[op]    (intlv) at (2.2,-4.0)  {Block Interleave\\60\,$\times$\,98};

      % Converge into frame assembly
      \node[block] (frame) at (0,-5.8)
           {Frame Assembly\\SP(68) + SB1(52) + IL(5880)\\= 6000\,symbols, 12\,s};

      \node[font=\small\bfseries] (out1) at (0, -7.1) {Encoded Frame};

      % Shared y-level for the horizontal segments entering frame
      \coordinate (join-y) at (0,-4.7);

      % Arrows — left column (vertical drop, right-angle into frame)
      \draw[arr] (sb1)  -- (bch);
      \draw[arr] (bch.south) -- (bch.south |- join-y) -| ([xshift=-5mm]frame.north);

      % Arrows — right column
      \draw[arr] (sb24) -- (crc);
      \draw[arr] (crc)  -- (ldpc);
      \draw[arr] (ldpc) -- (intlv);
      \draw[arr] (intlv.south) -- (intlv.south |- join-y) -| ([xshift=5mm]frame.north);

      % Output
      \draw[arr] (frame) -- (out1);

      \end{tikzpicture}
      \caption{Message encoding pipeline: navigation data to encoded frame.}
      \end{figure}

      \begin{figure}[ht]
      \centering
      \begin{tikzpicture}[
        block/.style={draw, rounded corners=2pt, minimum width=36mm,
                      minimum height=10mm, align=center, font=\small},
        op/.style={draw, rounded corners=2pt, minimum width=34mm,
                   minimum height=9mm, align=center, font=\small\itshape,
                   fill=black!5},
        arr/.style={-{Stealth[length=2.5mm]}, thick},
        every node/.style={font=\small},
      ]
      % --- Spreading & Modulation Pipeline ---
      \node[block] (enc) at (0, 0) {Encoded Frame\\6000 symbols, 500\,sym/s};

      \node[block] (afsi) at (-2.6, -1.8)
           {AFS-I Channel\\Data $\otimes$ Gold-2046\\BPSK @ 1.023\,Mcps};
      \node[block] (afsq) at ( 2.6, -1.8)
           {AFS-Q Channel\\Tiered Code (P$\oplus$S$\oplus$T)\\BPSK @ 5.115\,Mcps};

      \node[op] (iqmux) at (0, -3.6)
           {IQ Multiplexer\\I = AFS-I $\times$5$\uparrow$\quad Q = AFS-Q\\5.115\,MSPS, int16 I/Q};

      \node[font=\small\bfseries] (out2) at (0, -4.8) {Baseband I/Q Output};

      \draw[arr] (enc.south) -- ++(0,-0.3) -| (afsi.north);
      \draw[arr] (enc.south) -- ++(0,-0.3) -| (afsq.north);
      \draw[arr] (afsi.south) |- ([yshift=2mm]iqmux.west);
      \draw[arr] (afsq.south) |- ([yshift=2mm]iqmux.east);
      \draw[arr] (iqmux) -- (out2);

      \end{tikzpicture}
      \caption{Spreading and modulation pipeline: encoded frame to baseband I/Q.}
      \end{figure}

.. only:: html

   **Message Encoding Pipeline:**

   .. list-table::
      :widths: 25 15 60

      * - Navigation Message
        -
        -
      * - Subframe 1 (FID + TOI)
        - |rarr|
        - BCH(51,8): 9 bits |rarr| 52 symbols
      * - Subframes 2--4 (CED, Almanac, Access)
        - |rarr|
        - CRC-24Q (+24 bits) |rarr| LDPC rate 1/2
      * - Both paths merge
        - |rarr|
        - Block Interleave (60 |x| 98 matrix)
      * -
        - |rarr|
        - Frame Assembly: SP(68) + SB1(52) + Interleaved(5880) = **6000 symbols, 12 s**

   **Spreading & Modulation Pipeline:**

   .. list-table::
      :widths: 35 30 35

      * - **AFS-I Channel**
        -
        - **AFS-Q Channel**
      * - Data |otimes| Gold-2046
        -
        - Tiered Code (P |oplus| S |oplus| T)
      * - BPSK @ 1.023 Mcps
        -
        - BPSK @ 5.115 Mcps
      * -
        - |darr|
        -
      * -
        - **IQ Multiplexer** (I = AFS-I |times| 5 |uarr|, Q = AFS-Q) |rarr| 5.115 MSPS int16 I/Q
        -

   .. |otimes| unicode:: U+2297
   .. |oplus| unicode:: U+2295
   .. |darr| unicode:: U+2193
   .. |uarr| unicode:: U+2191
   .. |times| unicode:: U+00D7

----

.. _gd03:

Interface Definitions & Data Formats
-------------------------------------

A formal Interface Control Document (LUNALINK-ICD-001) defines:

- Public C++ API with full signature documentation per gateway
- Type-safe wrappers (``PrnId``, ``Fid``, ``Toi``) preventing misuse at compile time
- Python nanobind binding conventions
- Wire-level data formats (IQ interleave order, binary frame headers)
- Thread safety and reentrancy guarantees

Data format specifications for interoperability test vectors follow the
competition-defined standards:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Vector Type
     - Format
   * - Spreading codes
     - ``codes_prn{N}.hex`` — sectional ASCII hex
   * - Encoded frames
     - ``frame_{id}.bin`` — 64-byte metadata header + 6000 symbol bytes
   * - Baseband signals
     - ``signal_{prn}_{dur}s.iq`` — 128-byte header + float32 I/Q interleaved
       (competition format; int16 variant available for SDR interchange)
   * - Parsed data
     - ``parsed_{id}.json`` — structured field extraction

----

.. _gd04:

Technology Stack & Development Environment
-------------------------------------------

Technology Stack
~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Component
     - Details
   * - Language
     - C++20 (``constinit``, ``std::span``, ``std::popcount``)
   * - Build
     - CMake 3.15+ with Ninja; scikit-build-core for Python extension
   * - Hardening flags
     - ``-fno-exceptions -fno-rtti -fno-fast-math -fstack-protector-strong``
   * - Task runner
     - `Task <https://taskfile.dev>`_ — single command source of truth

Pre-commit Quality Pipeline
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Every ``git commit`` triggers a multi-step quality pipeline:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Step
     - Description
   * - ``build``
     - Compile C++ extension with hardening flags
   * - ``lint``
     - ruff + pyright + cppcheck
   * - ``test-cpp``
     - Catch2 unit tests via CTest
   * - ``test-py``
     - pytest across Python 3.12 / 3.13 / 3.14
   * - ``sanitize``
     - AddressSanitizer + UndefinedBehaviorSanitizer
   * - ``docs``
     - Sphinx with ``-W`` (warnings as errors)

Continuous Integration
~~~~~~~~~~~~~~~~~~~~~~

GitHub Actions workflows cover build, lint, test, coverage, static analysis,
sanitisers, and documentation deployment.  All workflows are blocking — a
failure on any gate prevents merge to main.

----

.. _gd05:

Testing Strategy & Validation Framework
----------------------------------------

Validation Hierarchy
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 8 20 72

   * - Level
     - Name
     - Scope
   * - 1
     - Unit Tests
     - Mathematical correctness of LFSRs, GF(2) operations, bit-packing
   * - 2
     - Component Tests
     - Per-gateway verification; generator vs Annex 3 bit-for-bit; phase alignment
   * - 3
     - Integration Tests
     - End-to-end TX |rarr| Channel |rarr| RX round-trip
   * - 4
     - Compliance Tests
     - Traceability to every ``shall`` requirement in LSIS V1.0
   * - 5
     - Interoperability Tests
     - Cross-decoding of standardised test vectors with independent implementations

.. |rarr| unicode:: U+2192

Test Infrastructure
~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Suite
     - Scope
   * - C++ (Catch2)
     - Per-module unit tests, integration round-trips, BER simulation
   * - Python (pytest)
     - Binding validation, test-vector generation, cross-version compatibility (3.12/3.13/3.14)
   * - CI Gates
     - Static analysis, coverage enforcement, sanitisers (ASan + UBSan), documentation build

Standardised Test Messages
~~~~~~~~~~~~~~~~~~~~~~~~~~

LunaLink defines five reference test messages for round-trip and
interoperability testing.  These are proposed as standardised test content
for cross-implementation validation during Phase 3:

.. list-table::
   :header-rows: 1
   :widths: 12 20 68

   * - ID
     - Name
     - Purpose
   * - TV-1
     - All Zeros
     - Baseline validation; verifies padding and CRC handling
   * - TV-2
     - All Ones
     - Inverse baseline; verifies scrambler and mapping logic
   * - TV-3
     - Alternating (0xAA...)
     - Verifies interleaver/deinterleaver matrix transpose
   * - TV-4
     - Ephemeris
     - Known CED parameters; verifies field bit-packing
   * - TV-5
     - Random (known seed)
     - Stress tests LDPC decoder convergence

----

.. _gd06:

Reference Data Validation
--------------------------

Both specification data annexes have been loaded, converted to native C++
data structures, and validated against the reference material.

Annex 2 — LDPC Submatrices
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 25 50

   * - Source
     - Internal Format
     - Validation
   * - 10 CSV files (SF2 + SF3, submatrices A--D + B\ :sup:`-1`)
     - Compressed Sparse Row (CSR)
     - Dimension and checksum validation on load

Annex 3 — PRN Spreading Codes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Family
     - Chips
     - PRNs
     - Validation
   * - Gold-2046
     - 2,046
     - 210
     - Generator output vs packed LUT, bit-for-bit (all 210 PRN IDs)
   * - Weil-10230
     - 10,230
     - 210
     - Aggregate CRC-32: ``0xffffffff780e10a7``
   * - Weil-1500
     - 1,500
     - 210
     - Aggregate CRC-32: ``0xffffffff031ee19d``

Table generation pipeline:

1. Raw hex from Annex 3 files |rarr| conversion script
2. MSB-first packing |rarr| C++ source files (``alignas(64)``, ``constinit``)
3. Build-time constant embedding with checksum validation

Pre-generated Test Vectors
~~~~~~~~~~~~~~~~~~~~~~~~~~

Test vectors in competition-standard formats are committed under ``test-vectors/``.
The initial set covers reference spreading codes (PRN 1--12) for all three
families (Gold-2046, Weil-10230, Weil-1500), validated bit-for-bit against
Annex 3.  Additional vectors for FEC and framing components will be published
as each gateway is completed.

Vectors are regenerable from source at any time and validated against the
live build during CI.

----

.. _gd07:

Compliance Validation Approach
-------------------------------

Compliance Matrix
~~~~~~~~~~~~~~~~~

A compliance matrix maps every LSIS V1.0 ``shall`` requirement to its
implementation file, test file, and verification status.  Requirements have
been extracted and catalogued for all eight gateways:

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Gateway
     - Requirements Identified
   * - Gateway 1 — Spreading Codes
     - 5 (Gold, Weil-10230, Weil-1500, matched-code, secondary codes)
   * - Gateway 2 — Forward Error Correction
     - 5 (BCH, LDPC, interleaver, CRC-24, rate matching)
   * - Gateway 3 — Message Framing
     - 3 (frame length, sync pattern, block interleaver offset)
   * - Gateway 4 — Baseband Signal Generation
     - 6 (BPSK-I, BPSK-Q, power sharing, alignment, tertiary sync)
   * - Gateways 5--8
     - Catalogued; to be detailed as implementation proceeds

Requirements Traceability
~~~~~~~~~~~~~~~~~~~~~~~~~

Every ``shall`` requirement in LSIS V1.0 is tagged with its source section
and linked to the corresponding module and test case.  Example traces:

.. list-table::
   :header-rows: 1
   :widths: 18 40 22 20

   * - Requirement
     - Description
     - Module
     - Test
   * - LSIS-221
     - AFS-I primary code shall be Gold-2046
     - ``prn``
     - ``test_gold.cpp``
   * - LSIS-222
     - AFS-Q primary code shall be Weil-10230
     - ``prn``
     - ``test_weil.cpp``
   * - LSIS-260
     - Each LNSP node linked to unique code set
     - ``matched_code``
     - ``test_matched.cpp``
   * - LSIS-FID0-440
     - SB2 encoded using rate-1/2 LDPC
     - ``ldpc``
     - ``test_ldpc.cpp``
   * - LSIS-FID0-470
     - Block interleaving per Table 17
     - ``interleaver``
     - ``test_interleaver.cpp``

Traceability is maintained as implementation proceeds through each gateway.

Configuration Management
~~~~~~~~~~~~~~~~~~~~~~~~~

Configuration is managed through git with the following controls:

- **Tagged baselines:** Each gateway exit is marked with an annotated git tag
  (e.g. ``gw1-baseline``), capturing a reproducible snapshot of source, tests,
  and documentation.
- **Branch protection:** The ``main`` branch requires passing CI and code
  review before merge; force-push is disabled.
- **Reproducible builds:** ``pyproject.toml`` and CMake version pins ensure
  deterministic dependency resolution.  ``task ci-checks`` reproduces the
  full CI pipeline locally.
- **Artefact traceability:** CI workflows publish build artefacts (test results,
  static-analysis reports, documentation) linked to the triggering commit SHA.

----

.. _gd08:

Roadmap
-------

.. list-table::
   :header-rows: 1
   :widths: 12 18 18 52

   * - Gateway
     - Status
     - Target
     - Scope
   * - G0
     - Complete
     - March 2026
     - Standards compliance and architecture (this document)
   * - G1
     - Design complete
     - April 2026
     - Spreading code generation and PRN table validation
   * - G2
     - Design complete
     - April 2026
     - Forward error correction (BCH, LDPC, CRC-24, interleaver)
   * - G3
     - In design
     - April 2026
     - Subframe builders, frame assembler
   * - G4
     - In design
     - May 2026
     - BPSK modulators, IQ multiplexer, signal file export
   * - G5
     - Planned
     - May 2026
     - Frame synchronisation, symbol extraction, FEC decoders
   * - G6
     - Planned
     - June 2026
     - Subframe parsers, Time-of-Transmission calculator
   * - G7
     - Planned
     - June 2026
     - End-to-end round-trip, interoperability testing, performance benchmarks
   * - G8
     - Planned
     - July 2026
     - API documentation, usage examples, compliance report

----

.. _appendix-build:

Appendix A: Build & Verification Instructions
----------------------------------------------

All deliverables can be verified by building and running the project from
source.

**Prerequisites:** Python 3.12+, CMake 3.15+, a C++20 compiler (GCC 12+ or
Clang 15+), and `Task <https://taskfile.dev>`_.

.. code-block:: bash

   task install          # create .venv, install all dependencies
   task install-hooks    # register pre-commit hook
   task ci-checks        # full local CI pipeline: build, lint, test, tidy, sanitize, docs

All gates must pass with exit code 0.

**Test vector regeneration:**

.. code-block:: bash

   python tests/generate_test_vectors.py    # regenerate all vectors
   python tests/validate_test_vectors.py    # verify against live build

----

.. _appendix-related:

Appendix B: Related Documents
------------------------------

The following documents are maintained in the project repository and provide
supporting detail for the claims in this report:

- **Spec Findings Report** (``docs/signal/spec_findings_report.rst``) — full rationale for each LSIS V1.0 finding
- **Architecture Description** (``docs/signal/architecture.rst``) — module layout and design rationale
- **Signal Chain Diagram** (``docs/signal/signal_chain.rst``) — annotated transmitter block diagram with data types and rates
- **Interface Control Document** (``docs/signal/icd.rst``, LUNALINK-ICD-001) — formal API specification
- **Validation Plan** (``docs/signal/validation_plan.rst``) — five-level test hierarchy with entry/exit criteria
- **Compliance Matrix** (``docs/signal/compliance_matrix.rst``) — full spec-to-code traceability
- **Developer Guide** (``docs/developing.rst``) — environment setup, task reference, contribution workflow
- **Technology Stack** (``docs/tech_stack.rst``) — tool versions and ESA Python guidelines compliance
- **Test Vector Format** (``docs/test_vectors.rst``) — interoperability vector specification
