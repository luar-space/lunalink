Development Status
==================

This document tracks the phased development of LunaLink from first PRN
codes to the complete, validated AFS-I and AFS-Q Signal-In-Space (SIS)
reference implementation delivered for the competition.

Phased Implementation
---------------------

.. list-table::
   :header-rows: 1
   :widths: 10 60 30

   * - Phase
     - Focus Area
     - Status
   * - **V1**
     - AFS-I Baseband Generation (PRN codes, BPSK modulation)
     - ✅ Completed
   * - **V2**
     - Complete AFS Transmitter (AFS-I/Q Mux, SB1 Encoder, Docs)
     - ✅ Completed
   * - **V3**
     - Full Codec implementation (BCH/LDPC codec, CRC-24, decode chain)
     - ✅ Completed
   * - **V4**
     - Validation & Interop (test vectors, BER curves, cross-implementation
       decode against LANS-AFS-SIM)
     - ✅ Completed
   * - **V5**
     - PNT Pipeline (constellation simulation, position fix)
     - 📅 Future work — beyond competition scope

Component Traceability
----------------------

.. list-table::
   :header-rows: 1
   :widths: 15 65 20

   * - ID
     - Component Description
     - Status
   * - **C1**
     - Spreading Code Loaders (Gold-2046, Weil-10230, Weil-1500)
     - ✅ Done
   * - **C2**
     - BPSK Modulators (AFS-I 1.023 MHz, AFS-Q 5.115 MHz)
     - ✅ Done
   * - **C3**
     - AFS-Q Matched-Code Combiner (Primary XOR Secondary XOR Tertiary)
     - ✅ Done
   * - **C4**
     - IQ Multiplexer (50/50 Power Sharing, Upsampling)
     - ✅ Done
   * - **C5-E**
     - BCH(51,8) Header Encoder
     - ✅ Done
   * - **C5-D**
     - BCH(51,8) ML Correlation Decoder (Radiation-Hardened)
     - ✅ Done
   * - **C6**
     - LDPC Encoders (5G NR base graph 2; SF2 1200/2400, SF3/SF4 870/1740)
     - ✅ Done
   * - **C7**
     - LDPC Decoder (layered sum-product belief propagation)
     - ✅ Done
   * - **C8**
     - Navigation Frame Builder (sync, SB1, 60×98 block interleaving)
     - ✅ Done
   * - **C9**
     - Navigation Message Parsing (FID/TOI, WN/ITOW, time of transmission;
       SB3/SB4 payloads as raw bit-slices per the V1.0 TBW field layouts)
     - ✅ Done

Deliverables Status
-------------------

*   **Standards compliance**: mandatory LSIS V1.0 requirements traced and
    implemented — see :doc:`signal/compliance_matrix` and
    :doc:`signal/shall_inventory`.
*   **Architecture documentation**: complete design record for all core
    modules (:doc:`gateway0`, :doc:`signal/architecture`).
*   **Mission assurance**: 100% Python line coverage (CI-gated); C++
    coverage and MC/DC evidence per :doc:`assurance/mission_assurance`.
*   **Interoperability**: bit-exact cross-check and signal cross-decode
    against LANS-AFS-SIM (:doc:`interop_results`); test vectors published
    at `luar-space/lsis-afs-test-vectors
    <https://github.com/luar-space/lsis-afs-test-vectors>`__.
