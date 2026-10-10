Development Status
==================

LunaLink grew in phases, from the first PRN codes to the validated AFS-I and
AFS-Q Signal-In-Space (SIS) reference implementation delivered for the
competition.

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

Post-submission work
--------------------

Named here so the boundary of what has been demonstrated is explicit:

1. **Interoperability beyond SB2 and beyond baseband.**  Both directions are
   now demonstrated (:doc:`interop_results` L1-L5): LunaLink decodes
   LANS-AFS-SIM's signal, and PocketSDR-AFS decodes LunaLink's.  What remains
   is breadth — comparing SB3 and SB4 as well as SB2, which needs a receiver
   that logs them in decoded form, and running the reverse direction against
   a captured RF signal rather than a generated baseband one.
2. **Hardware-in-the-loop verification.**  The HAL_STUB coverage waivers in
   :doc:`assurance/coverage_exclusions` resolve at TRL-5 on target
   hardware (PolarFire SoC).
3. **Effective-coverage gate promotion.**  ``task coverage-effective`` is
   short of 100% repository-wide: a few dozen uncovered lines still need an
   individual disposition (test it, or mark it with a justified category)
   before the check can become a blocking gate in ``task ci-checks``.  The
   current figure and the list of open lines are kept in one place,
   :doc:`assurance/coverage_exclusions`.  The
   spreading-code generators in ``cpp/codes/`` are already at 100.0% with
   zero unmarked lines (:doc:`gateway1`).
4. **PNT pipeline (V5).**  Constellation simulation and position fix, as
   above.
5. **Sample-clock-offset and phase-noise sweeps.**  The validation plan
   names them; neither is implemented, and the receiver has no delay-locked
   loop to compensate clock drift.  Quantisation and burst-error impairments
   are tested end to end (``test_e2e_impairments.cpp``); Doppler is
   exercised only through the L3 cross-decode of a foreign signal.

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
