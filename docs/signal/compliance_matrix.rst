Compliance Matrix
=================

This document tracks the compliance of the LunaLink implementation with the 
`Lunar Net Signal-In-Space (LSIS) Recommended Standard <https://github.com/fiskio/lunalink/blob/main/docs/references/lunanet-signal-in-space-recommended-standard-augmented-forward-signal-vol-a.pdf>`_ 
(Vol-A, Jan 29, 2025). See :doc:`../references` for more context.

Status Legend:
  * ✅ **Compliant**: Fully implemented and verified.
  * ⚠️ **Partial**: Partially implemented or placeholder used.
  * ❌ **Not Started**: Implementation pending.
  * ℹ️ **N/A**: Not applicable to this software implementation.

Signal Structure (Section 2.3)
------------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 60 10 15

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-101
     - AFS-I BPSK(1) modulation at 1.023 Mcps
     - ✅
     - Implemented in ``modulator.cpp``
   * - LSIS-102
     - AFS-Q BPSK(5) modulation at 5.115 Mcps
     - ✅
     - Implemented in ``modulator.cpp``
   * - LSIS-103
     - 50/50 Power Sharing between I and Q
     - ✅
     - Implemented in ``iq_mux.cpp``
   * - LSIS-110
     - Gold-2046 Spreading Codes (210 IDs)
     - ✅
     - Packed storage in ``prn_table.cpp``
   * - LSIS-120
     - Weil-10230 Spreading Codes (210 IDs)
     - ✅
     - Packed storage in ``prn_table_weil10230.cpp``
   * - LSIS-130
     - Weil-1500 Spreading Codes (210 IDs)
     - ✅
     - Packed storage in ``prn_table_weil1500.cpp``
   * - LSIS-140
     - Matched-Code Combination (Primary XOR Secondary XOR Tertiary)
     - ✅
     - Implemented in ``matched_code.cpp``
   * - LSIS-150
     - Secondary Code S0=1110, S1=0111, S2=1011, S3=1101
     - ✅
     - Implemented in ``matched_code.cpp``
   * - LSIS-260
     - Default Interim Code Assignment (Node 1-12)
     - ✅
     - Implemented in ``matched_code.cpp``

Navigation Message (Section 2.4)
--------------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 60 10 15

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-301
     - Frame Length = 12.0 seconds (6000 symbols)
     - ✅
     - Verified in ``test_frame.cpp``
   * - LSIS-302
     - 68-symbol Sync Pattern (CC63F74536F49E04Ah)
     - ✅
     - Implemented in ``frame.cpp``
   * - LSIS-310
     - BCH(51,8) SB1 Encoding
     - ✅
     - Implemented in ``bch.cpp``
   * - LSIS-311
     - BCH(51,8) SB1 Decoding (ML Correlation)
     - ✅
     - Flight-hardened implementation in ``bch.cpp``
   * - LSIS-320
     - LDPC(1200,600) for SB2-SB4 (SF2, SF3, SF4)
     - ❌
     - Planned for V3
   * - LSIS-330
     - Interleaving for SB2-SB4
     - ❌
     - Planned for V3
   * - LSIS-340
     - CRC-24 for Message Integrity
     - ❌
     - Planned for V3

TBD/TBC Tracking
----------------

.. list-table::
   :header-rows: 1
   :widths: 15 60 10 15

   * - ID
     - Description
     - Status
     - Notes
   * - LSIS-TBD-2001
     - Final matched-code assignment and tertiary phasing
     - ⚠️
     - Using interim mapping per §2.3.5.4
   * - LSIS-TBC-3001
     - SB2-SB4 Bit Layouts for TBW Messages
     - ⚠️
     - SISICD draft in progress
