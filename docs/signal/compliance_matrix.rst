Compliance Matrix
=================

This matrix tracks the implementation status of every requirement in the LSIS-AFS Specification (LSIS V1.0), organized by the competition's 8-Gateway structure.

Gateway 1: Spreading Code Generation
------------------------------------
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
     - Implemented in ``prn_table.cpp``
   * - LSIS-120
     - Weil-10230 Spreading Codes (210 IDs)
     - ✅
     - Implemented in ``prn_table_weil10230.cpp``
   * - LSIS-130
     - Weil-1500 Spreading Codes (210 IDs)
     - ✅
     - Implemented in ``prn_table_weil1500.cpp``
   * - LSIS-140
     - Matched-Code Combination (Primary XOR Secondary XOR Tertiary)
     - ✅
     - Implemented in ``matched_code.cpp``
   * - LSIS-150
     - Secondary Code S0=1110, S1=0111, S2=1011, S3=1101
     - ✅
     - Implemented in ``matched_code.cpp``

Gateway 2: Forward Error Correction
-----------------------------------
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
     - Implemented in ``bch.cpp``
   * - LSIS-311
     - BCH(51,8) SB1 Decoding (ML Correlation)
     - ✅
     - Implemented in ``bch.cpp``
   * - LSIS-320
     - LDPC(1200,600) for SB2-SB4 (SF2, SF3, SF4)
     - ❌
     - Planned for Phase 2
   * - LSIS-330
     - Interleaving for SB2-SB4
     - ❌
     - Planned for Phase 2
   * - LSIS-340
     - CRC-24 for Message Integrity
     - ❌
     - Planned for Phase 2

Gateway 3: Message Framing
--------------------------
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
     - 60x98 Block Interleaver for symbols 121-6000
     - TODO
     - G3.2

Gateway 4: Baseband Signal Generation
-------------------------------------
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
     - Implemented in ``modulator.cpp``
   * - LSIS-102
     - AFS-Q BPSK(5) modulation at 5.115 Mcps
     - ✅
     - Implemented in ``modulator.cpp``
   * - LSIS-103
     - 50/50 Power Sharing between I and Q
     - ✅
     - Implemented in ``iq_mux.cpp``
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

Gateway 6: Message Parsing & Time
---------------------------------
.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-720
     - Calculate Time of Transmission (ToT)
     - TODO
     - G6.2
   * - LSIS-FIG6
     - MSB-first bit and byte ordering
     - ✅
     - Verified in serialization tests

Gateway 7: Validation & Utilities
---------------------------------
.. list-table::
   :widths: 15 60 10 15
   :header-rows: 1

   * - ID
     - Requirement
     - Status
     - Notes
   * - LSIS-SISE
     - Implement SISE position/velocity error calc
     - TODO
     - Phase 7.1
   * - LSIS-TABLE11
     - All 12 interim test codes working (PRN 1-12)
     - ✅
     - Verified in spreading tests


(Additional Gateways G5-G8 to be populated as implementation proceeds)
