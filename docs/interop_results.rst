Cross-Implementation Interoperability Results
=============================================

.. rubric:: LUNALINK-INTEROP-001

**Date:** 2026-03-24

**Implementations under test:**

- **LunaLink** v0.1.22 — C++ / Python LSIS-AFS reference implementation
- **LANS-AFS-SIM** (Ebinuma) — C, BSD-2-Clause, https://github.com/osqzss/LANS-AFS-SIM

**Reference:** Sobukawa & Ebinuma, *Aerospace* 12(7), 2025

Summary
-------

.. list-table::
   :header-rows: 1
   :widths: 10 30 15 45

   * - Level
     - Test
     - Result
     - Evidence
   * - **L1**
     - Gold code bit-exact (PRN 1–12)
     - **PASS**
     - 0/2046 chip mismatches per PRN
   * - **L1**
     - Weil-10230 code bit-exact (PRN 1–12)
     - **PASS**
     - 0/10230 chip mismatches per PRN
   * - **L2**
     - Frame encoding (5 payloads × 3 patterns)
     - **PASS**
     - 0/6000 symbol mismatches per frame (BCH, CRC-24, LDPC, interleaver)
   * - **L3**
     - PCPS acquisition of LANS-AFS-SIM signal
     - **PASS**
     - C++ correlator locks (PRN 3, Doppler ≈ 0 Hz)
   * - **L3**
     - Full frame decode from LANS-AFS-SIM I/Q
     - **PASS**
     - FID=0, WN=2277, SB2/SB3/SB4 CRC-24 verified

Methodology
-----------

**L1 — Spreading code comparison**

LANS-AFS-SIM's ``icodegen()`` and ``qcodegen()`` functions were called via
a C harness (``dump_lans_codes.c``) to extract Gold-2046 and Weil-10230 codes
for PRN 1–12.  Each code was compared chip-by-chip against LunaLink's packed
PRN tables.  Both implementations use independent code generation:
LANS-AFS-SIM uses an 11-bit LFSR with a delay table; LunaLink uses
pre-computed tables generated from the LSIS V1.0 Appendix C/D parameters.

**L2 — Frame encoding comparison**

A C harness (``dump_lans_frame.c``) invokes LANS-AFS-SIM's encoding pipeline
(``generate_BCH_AFS_SF1``, ``append_CRC24``, ``encode_LDPC_AFS_SF2/SF3``,
``interleave_AFS_SF234``) with controlled payloads (all-zeros, all-ones,
alternating, boundary FID/TOI values).  The resulting 6000-symbol frames
are compared against LunaLink's ``frame_build()`` output.

**L3 — Cross-implementation signal decode**

LANS-AFS-SIM generates a 25-second composite I/Q signal at 5.115 MHz
containing 7 SVs (PRN 2–8) with Keplerian ephemeris from a lunar NRHO-like
constellation.  The signal is decimated to 1.023 MSPS (1 sample/chip) and
fed through two independent receiver paths:

1. **Python despreading** with PLL carrier tracking → ``receive_frame()``
2. **C++ Correlator** (PCPS acquisition + DLL/PLL tracking) → ``data_symbol()``
   → ``receive_frame()``

Both paths successfully decode the navigation frame: FID, TOI, WN/ITOW
extraction, and CRC-24 verification on all three subframes.

Test infrastructure
-------------------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - File
     - Purpose
   * - ``scripts/interop/build_lans_afs_sim.py``
     - Clone and build LANS-AFS-SIM + interop harnesses
   * - ``scripts/interop/generate_lans_signal.py``
     - Generate I/Q signal from LANS-AFS-SIM
   * - ``scripts/interop/dump_lans_codes.c``
     - C harness: extract PRN codes
   * - ``scripts/interop/dump_lans_frame.c``
     - C harness: encode frames with controlled payloads
   * - ``scripts/interop/validate_foreign_vectors.py``
     - Format validator for any team's test vectors
   * - ``tests/test_interop_codes.py``
     - L1 pytest: Gold + Weil code comparison
   * - ``tests/test_interop_frames.py``
     - L2 pytest: frame encoding comparison
   * - ``tests/test_interop_decode.py``
     - L3 pytest: signal acquisition, tracking, and frame decode

Reproduction
------------

.. code-block:: bash

   # 1. Build LANS-AFS-SIM and interop harnesses
   python scripts/interop/build_lans_afs_sim.py

   # 2. Run all interop tests
   pytest tests/test_interop_codes.py tests/test_interop_frames.py \
          tests/test_interop_decode.py tests/test_interop_self.py -v

Known limitations
-----------------

**Correlator sample rate.** The current C++ correlator operates at 1.023 MSPS
(1 sample per I-channel chip).  This decimates the composite signal from its
native bandwidth (set by AFS-Q at 5.115 Mcps), aliasing the Q-channel pilot
as broadband interference on the I channel.  Under low-SNR conditions (2-bit
ADC, multiple SVs), this aliasing degrades acquisition sensitivity.

A flight-credible receiver should operate at the composite signal rate
(≥ 10.23 MSPS) and implement pilot-channel (AFS-Q) acquisition, which
provides data-bit-free coherent integration over the secondary code period
(8 ms) or longer.  This is planned for the correlator V2 redesign.

**Non-coherent integration.** The correlator supports multi-epoch non-coherent
integration (configurable N, up to 20 epochs / 40 ms) for improved acquisition
sensitivity on the data channel.  This provides ~√N gain (5 dB at N=10) and
is sufficient for clean signals.  For the challenging 2-bit multi-SV scenario,
pilot-channel acquisition (V2) is the correct architectural solution.

