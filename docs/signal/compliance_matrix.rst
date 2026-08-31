Compliance Matrix
=================

This matrix tracks the implementation status of every requirement in the
LSIS-AFS Specification (LSIS V1.0), organized by the competition's
8-Gateway structure.

Gateway 1: Spreading Code Generation
-------------------------------------

.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-110
     - Gold-2046 Spreading Codes (210 IDs)
     - ✅
     - Hardened C++ Generator (``gold.cpp``) + Packed LUTs
   * - LSIS-120
     - Weil-10230 Spreading Codes (210 IDs)
     - ✅
     - Hardened C++ Generator (``weil.cpp``) + Packed LUTs
   * - LSIS-130
     - Weil-1500 Spreading Codes (210 IDs)
     - ✅
     - Hardened C++ Generator (``weil.cpp``) + Packed LUTs
   * - LSIS-140
     - Matched-Code Combination (Primary XOR Secondary XOR Tertiary)
     - ✅
     - Implemented [Hardened] in ``matched_code.cpp``
   * - LSIS-150
     - Secondary Code S0=1110, S1=0111, S2=1011, S3=1101
     - ✅
     - Implemented [Hardened] in ``matched_code.cpp``

Gateway 2: Forward Error Correction
------------------------------------

.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-310
     - BCH(51,8) SB1 Encoding
     - ✅
     - Implemented [Hardened] in ``bch.cpp``
   * - LSIS-311
     - BCH(51,8) SB1 Decoding (ML Correlation)
     - ✅
     - Implemented [Hardened]; ML verified
   * - LSIS-320
     - LDPC(1200,600) Encoder for SB2-SB4 (SF2, SF3, SF4)
     - ✅
     - Implemented [Hardened]; CSR-hardened; H·c=0 self-check
   * - LSIS-321
     - LDPC(1200,600) Decoder (Layered NMS BP, Q8.7 fixed-point)
     - ✅
     - Implemented [Hardened]; NMS α=0.75; BER waterfall verified
   * - LSIS-330
     - Block Interleaving for SB2-SB4 (60×98)
     - ✅
     - Implemented [Hardened] in ``interleaver.cpp``; CFI sentinels
   * - LSIS-331
     - Block De-interleaving (soft LLR, 60×98 column-to-row transpose)
     - ✅
     - Implemented [Hardened] in ``interleaver.cpp``; volatile CFI
   * - LSIS-340
     - CRC-24 for Message Integrity
     - ✅
     - Implemented [Hardened] CRC-24Q (0x864CFB)

Gateway 3: Message Framing
---------------------------

.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-301
     - Frame Length = 12.0 seconds (6000 symbols)
     - ✅
     - Verified in ``test_frame.cpp``
   * - LSIS-340
     - 68-symbol synchronization pattern (uncoded)
     - ✅
     - Implemented in ``frame.cpp``
   * - LSIS-FID0-470
     - 60×98 Block Interleaver for symbols 121–6000
     - ✅
     - Implemented [Hardened] in ``interleaver.cpp``

Gateway 4: Baseband Signal Generation
--------------------------------------

.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-101
     - AFS-I BPSK(1) modulation at 1.023 Mcps
     - ✅
     - Implemented [Hardened] in ``modulator.cpp``
   * - LSIS-102
     - AFS-Q BPSK(5) modulation at 5.115 Mcps
     - ✅
     - Implemented [Hardened] in ``modulator.cpp``
   * - LSIS-103
     - 50/50 Power Sharing between I and Q
     - ✅
     - Implemented [Hardened] in ``iq_mux.cpp``
   * - LSIS-160
     - Align data symbols with primary code boundaries
     - ✅
     - Implemented in ``modulator.cpp``
   * - LSIS-171
     - Align tertiary code with secondary code boundaries
     - ✅
     - Implemented in ``matched_code.cpp``
   * - LSIS-220
     - Synchronize tertiary start with frame start
     - ✅
     - Implemented in ``matched_code.cpp``

Gateway 5: Frame Sync & Decoding
---------------------------------

.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-710
     - Sync Pattern Detection — 68-symbol cross-correlation
     - ✅
     - Implemented [Hardened] in ``sync_detector.cpp``; TMR peak; CFI
   * - LSIS-711
     - Frame Decode Orchestrator (sync → BCH → deinterleave → LDPC → CRC)
     - ✅
     - Implemented [Hardened] in ``frame_decode.cpp``; 6-milestone CFI

Gateway 6: Message Parsing & Time
----------------------------------

.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-601
     - Message Parsers (8 spec-defined + 14 SISICD types)
     - ✅
     - Implemented in ``message_parser.cpp``; SB2 WN/ITOW, SB3/SB4 type routing
   * - LSIS-720
     - Calculate Time of Transmission (ToT)
     - ✅
     - Implemented [Hardened] in ``time_of_transmission.cpp``; CFI sentinels
   * - LSIS-FIG6
     - MSB-first bit and byte ordering
     - ✅
     - Verified in serialization tests

Gateway 7: Validation & Utilities
----------------------------------

.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-SISE
     - Implement SISE position/velocity error calc
     - ✅
     - Implemented [Hardened] in ``sise.cpp``; RSS + 95th percentile; 40 m / 0.01 m/s limits
   * - LSIS-TABLE11
     - All 12 interim test codes working (PRN 1-12)
     - ✅
     - Verified in spreading tests
   * - LSIS-E2E
     - End-to-end round-trip: encode → signal → decode → verify
     - ✅
     - 9 E2E tests in ``test_e2e.cpp``; PRN 1-12; FER sweep 3 SNR points
   * - LSIS-INTEROP
     - Interoperability test vectors (5 std messages, PRN 1-12)
     - ✅
     - ``generate_test_vectors.py``; levels 1-5 (codes → signal → parse)
   * - LSIS-PERF
     - Performance benchmarks (throughput, latency, real-time)
     - ✅
     - Frame decode < 1 s; WCET profiling; BER waterfall; quantization

Gateway 8: Documentation & Examples
------------------------------------

.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-DOC-API
     - API Reference (Sphinx + Doxygen)
     - Partial
     - Ongoing; Sphinx deployed to gh-pages
   * - LSIS-DOC-EXAMPLES
     - Usage Examples (Notebooks)
     - TODO
     - GD8.2
   * - LSIS-DOC-COMPLIANCE
     - Final Compliance Matrix
     - Partial
     - This document
   * - LSIS-DOC-FINDINGS
     - Spec Findings Report
     - Partial
     - ``spec_findings_report.rst``
