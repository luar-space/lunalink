.. meta::
   :description: Gateway 0 (Design & Architecture) and Milestone 1 deliverable for the ESA-CCSDS Protocol Development Competition

=================================================================================
Gateway 0: Design & Architecture --- Standards Compliance & Architecture
=================================================================================

.. note::

   **Historical document.**  This was written for the ESA-CCSDS Protocol
   Development Outreach Competition, which LunaLink won in September 2026.
   It is kept as a record and is no longer maintained, so its figures,
   counts and open items describe the project at that time.  For the current design, see :doc:`/signal/architecture` and
   :doc:`/verifying`.

This document serves two deliverables at once: **Gateway 0 (Design &
Architecture)** of the competition rubric, and the **Milestone 1** standards
compliance and architecture deliverable of the ESA-CCSDS Delivery Outline.
The Gateway 0 checklist is mapped in `Gateway 0 deliverables`_ below.

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

Studying the specification turned up three gaps in LSIS V1.0, each documented
here with a proposed resolution.  The architecture centres on a deterministic
C++ core — no exceptions, no RTTI, no dynamic allocation on the signal path —
wrapped by Python bindings for prototyping and test-vector generation.  Both
data annexes (Annex 2: LDPC submatrices, Annex 3: PRN spreading codes) load
into native data structures and check against the reference material.

----

.. _gateway 0 deliverables:

Gateway 0 deliverables
----------------------

The rubric lists eight Gateway 0 deliverables.  Each maps to the section
that carries it.

.. list-table::
   :header-rows: 1
   :widths: 38 30 32

   * - Rubric deliverable
     - Section
     - Also see
   * - System architecture document
     - `System Architecture & Code Structure`_
     - :doc:`/signal/architecture`
   * - Component interaction diagrams
     - `Signal Chain`_ (encode and decode pipelines)
     - :doc:`/signal/signal_chain`
   * - Technology stack selection
     - `Technology Stack`_
     - :doc:`/developing`
   * - Development environment setup
     - `Technology Stack & Development Environment`_,
       `Continuous Integration`_
     - :doc:`/quickstart`, :doc:`/developing`
   * - Testing strategy and framework
     - `Testing Strategy & Validation Framework`_
     - :doc:`/signal/validation_plan`
   * - Code structure and module design
     - `Module Design`_
     - :doc:`/signal/architecture`
   * - Interface definitions
     - `Interface Definitions & Data Formats`_
     - :doc:`/signal/icd`
   * - Data format specifications
     - `Interface Definitions & Data Formats`_
     - :doc:`/test_vectors`, :doc:`/signal/sisicd`

**Success criteria.**  Architecture defined with clear components
(`Module Design`_); development environment ready and reproducible
(``task install``, :doc:`/quickstart`); testing framework established
(`Test Infrastructure`_ — Catch2 for C++, pytest for Python, both wired
into blocking CI gates).

----

.. _gd01:

Specification Analysis
----------------------

Three gaps in LSIS V1.0 force an implementation assumption before
interoperability can be deterministic.  Each finding names the affected
section, the risk it creates, and the resolution LunaLink adopted.

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
     - LunaLink emits the interchange shape used at the interoperability
       workshop: headerless interleaved ``float32`` little-endian
       ``[I0, Q0, I1, Q1, ...]`` (``lunalink encode --format iq32``), plus a
       self-describing ``LSISIQ``-headed variant for archival use.  Both are
       documented in :doc:`/test_vectors`.

.. |S| unicode:: U+00A7

The project documentation carries the full findings report, with rationale
and standard references.

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

The split lets the core drop untouched into an embedded or flight-software
context, while the Python layer keeps it reachable for researchers and
integrators.

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

The transmitter carries navigation data from message bits through FEC
encoding, interleaving, spreading, modulation, and IQ multiplexing.  Two
pipelines follow: message encoding, from navigation data to encoded frame,
then spreading and modulation, from encoded frame to baseband I/Q.

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

Interoperability test vectors follow the competition-defined formats:

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
interoperability testing, offered as standardised content for
cross-implementation validation in Phase 3:

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

Both data annexes load into native C++ structures and check against the
reference material.

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
     - Aggregate CRC-32: ``0x780e10a7`` (carried as ``uint64_t``
       ``0xffffffff780e10a7`` in ``codebook_checksums.hpp``)
   * - Weil-1500
     - 1,500
     - 210
     - Aggregate CRC-32: ``0x031ee19d`` (carried as ``uint64_t``
       ``0xffffffff031ee19d`` in ``codebook_checksums.hpp``)

Table generation pipeline:

1. Raw hex from Annex 3 files |rarr| conversion script
2. MSB-first packing |rarr| C++ source files (``alignas(64)``, ``constinit``)
3. Build-time constant embedding with checksum validation

Pre-generated Test Vectors
~~~~~~~~~~~~~~~~~~~~~~~~~~

Competition-standard test vectors live under ``test-vectors/``
(``codes/``, ``fec/``, ``frames/``, ``signals/``, ``parsed/``, ``decode/``).
The spreading-code set covers all 210 PRNs for all three families (Gold-2046,
Weil-10230, Weil-1500), validated bit-for-bit against Annex 3; FEC, frame,
signal, and decode-direction vectors are committed alongside them.  All of it
is published at `luar-space/lsis-afs-test-vectors
<https://github.com/luar-space/lsis-afs-test-vectors>`__.

Any vector regenerates from source on demand, and CI checks the set against
the live build.

----

.. _gd07:

Compliance Validation Approach
-------------------------------

Compliance Matrix
~~~~~~~~~~~~~~~~~

A compliance matrix maps every LSIS V1.0 ``shall`` requirement to its
implementation file, test file, and verification status.  All eight gateways
are catalogued:

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
     - ``cpp/tests/codes/test_prn_generators.cpp``
   * - LSIS-222
     - AFS-Q primary code shall be Weil-10230
     - ``prn``
     - ``cpp/tests/codes/test_prn_generators.cpp``
   * - LSIS-260
     - Each LNSP node linked to unique code set
     - ``matched_code``
     - ``cpp/tests/codes/test_matched_code.cpp``
   * - LSIS-FID0-440
     - SB2 encoded using rate-1/2 LDPC
     - ``ldpc``
     - ``test_ldpc.cpp``
   * - LSIS-FID0-470
     - Block interleaving per Table 17
     - ``interleaver``
     - ``test_interleaver.cpp``

Traceability grows with the implementation, gateway by gateway.

Configuration Management
~~~~~~~~~~~~~~~~~~~~~~~~~

Git carries configuration management, under four controls:

- **Tagged baselines:** Each gateway exit is marked with an annotated git tag
  (e.g. ``gw1-baseline``), capturing a reproducible snapshot of source, tests,
  and documentation.
- **Branch protection:** The ``main`` branch requires passing CI and code
  review before merge; force-push is disabled.
- **Reproducible builds:** version pins in ``pyproject.toml`` and CMake make
  dependency resolution deterministic.  ``task ci-checks`` reproduces the
  full CI pipeline locally.
- **Artefact traceability:** CI workflows publish build artefacts (test results,
  static-analysis reports, documentation) linked to the triggering commit SHA.

----

.. _gd08:

Roadmap
-------

*Statuses below are the planning snapshot as of 31 March 2026 — see*
:doc:`/roadmap` *for the delivered status; G8 was delivered as* :doc:`/api`
*and* :doc:`/history/evaluation_guide`\ *.*

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

Building and running the project from source verifies every deliverable.

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
   uv run pytest tests/test_interop_self.py # validate vectors against live build

----

.. _appendix-related:

Appendix B: Related Documents
------------------------------

These repository documents carry the supporting detail behind the claims
above:

- **Spec Findings Report** (``docs/signal/spec_findings_report.rst``) — full rationale for each LSIS V1.0 finding
- **Architecture Description** (``docs/signal/architecture.rst``) — module layout and design rationale
- **Signal Chain Diagram** (``docs/signal/signal_chain.rst``) — annotated transmitter block diagram with data types and rates
- **Interface Control Document** (``docs/signal/icd.rst``, LUNALINK-ICD-001) — formal API specification
- **Validation Plan** (``docs/signal/validation_plan.rst``) — five-level test hierarchy with entry/exit criteria
- **Compliance Matrix** (``docs/signal/compliance_matrix.rst``) — full spec-to-code traceability
- **Developer Guide** (``docs/developing.rst``) — environment setup, task reference, contribution workflow
- **Technology Stack** (``docs/tech_stack.rst``, since folded into ``docs/developing.rst``) — tool versions and ESA Python guidelines compliance
- **Test Vector Format** (``docs/test_vectors.rst``) — interoperability vector specification
