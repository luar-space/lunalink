LSIS V1.0 Shall-Statement Inventory
====================================

:Document ID: LUNALINK-SHALL-001
:Spec Reference: LSIS V1.0, 29 January 2025
:Generated: 2026-03-23

Every normative requirement identifier in LSIS V1.0, mapped to a disposition
for the LunaLink reference implementation.

Disposition Key
---------------

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Disposition
     - Meaning
   * - **Implemented**
     - Code exists, tests pass, traceability tag present
   * - **RF / Out of Scope**
     - Requires RF hardware — competition spec explicitly excludes
   * - **TBW in Spec**
     - Spec marks content {LSIS-TBW-nnnn} — cannot implement
   * - **TBC in Spec**
     - Spec marks values {LSIS-TBC-nnnn} — implemented with documented assumption
   * - **TBD marker** {LSIS-TBD-nnnn}
     - Cited in evidence cells: spec leaves the value To Be Determined —
       implemented with a documented interim assumption (see
       :doc:`spec_findings_report`)
   * - **FLEX**
     - Spec marks as LNSP implementation flexibility — documented in SISICD

Summary
-------

.. list-table::
   :widths: 30 15
   :header-rows: 1

   * - Disposition
     - Count
   * - Implemented
     - 49
   * - RF / Out of Scope
     - 12
   * - TBW in Spec
     - 10
   * - TBC in Spec (implemented with assumption)
     - 3
   * - FLEX
     - 2
   * - **Total**
     - **76**

----

Section 2.2 --- Signal-In-Space Error
--------------------------------------

.. list-table::
   :widths: 15 8 40 18 19
   :header-rows: 1

   * - LSIS ID
     - Sec.
     - Requirement
     - Disposition
     - Evidence
   * - LSIS-001
     - 2.2.3
     - SISE pos |le| 40 m (95th percentile, 24 h)
     - Implemented
     - ``sise.cpp``, ``test_sise.cpp`` [LSIS-SISE]
   * - LSIS-002
     - 2.2.3
     - SISE vel |le| 0.01 m/s (95th percentile, 24 h)
     - Implemented
     - ``sise.cpp``, ``test_sise.cpp`` [LSIS-SISE]
   * - LSIS-005
     - 2.2.3
     - Predicted SISE in MSG-G4 (SISEpos + SISEvel)
     - TBW in Spec
     - MSG-G4 content is {LSIS-TBW-2006}
   * - LSIS-006
     - 2.2.3
     - Accuracy of predicted SISE position
     - TBW in Spec
     - {LSIS-TBW-2001}
   * - LSIS-007
     - 2.2.3
     - Accuracy of predicted SISE velocity
     - TBW in Spec
     - {LSIS-TBW-2002}

Section 2.3.1 --- Composite Signal
------------------------------------

.. list-table::
   :widths: 15 8 40 18 19
   :header-rows: 1

   * - LSIS ID
     - Sec.
     - Requirement
     - Disposition
     - Evidence
   * - LSIS-010
     - 2.3.1.1
     - Frequency band S-band 2483.5--2500 MHz
     - RF / Out of Scope
     - Baseband only; no carrier generation
   * - LSIS-020
     - 2.3.1.1
     - Carrier frequency 2492.028 MHz
     - RF / Out of Scope
     - Baseband only
   * - LSIS-030
     - 2.3.1.1
     - Carrier frequency max offset < 10 Hz
     - RF / Out of Scope
     - Baseband only
   * - LSIS-040
     - 2.3.1.2
     - Signal polarization RHCP
     - RF / Out of Scope
     - Antenna design excluded
   * - LSIS-050
     - 2.3.1.2
     - RHCP axial ratio < 3 dB {LSIS-TBC-2005}
     - RF / Out of Scope
     - Antenna design excluded
   * - LSIS-060
     - 2.3.1.3
     - Phase noise spectral density (PLL tracking)
     - RF / Out of Scope
     - Transmitter hardware excluded
   * - LSIS-070
     - 2.3.1.4
     - Max in-band spurious emissions |le| -35 dB
     - RF / Out of Scope
     - Transmitter hardware excluded
   * - LSIS-080
     - 2.3.1.4
     - Max out-of-band emissions
     - TBW in Spec
     - {LSIS-TBW-2003}
   * - LSIS-090
     - 2.3.1.5
     - Receiver reference bandwidth 15.944 MHz
     - RF / Out of Scope
     - Receiver front-end excluded
   * - LSIS-100
     - 2.3.1.5
     - Correlation loss < 0.6 dB {LSIS-TBC-2009}
     - RF / Out of Scope
     - Transmitter hardware excluded
   * - LSIS-103
     - 2.3.1.6
     - Power ratio AFS-I/AFS-Q = 50/50 (Table 3)
     - Implemented
     - ``iq_mux.cpp`` equal power split [LSIS-AFS-301]
   * - LSIS-105
     - 2.3.1.8
     - LNSP-defined service volume (FLEX)
     - FLEX
     - LNSP implementation flexibility; documented
   * - LSIS-110
     - 2.3.1.7
     - Min/max received power on lunar surface (Table 4)
     - RF / Out of Scope
     - Link budget; no transmitter
   * - LSIS-120
     - 2.3.1.8
     - Max power outside service volume (Table 5)
     - RF / Out of Scope
     - Link budget; no transmitter
   * - LSIS-125
     - 2.3.2
     - Signal components linearly multiplexed (Figure 2)
     - Implemented
     - ``iq_mux.cpp`` [LSIS-AFS-301]

Section 2.3.2 --- Modulation
------------------------------

.. list-table::
   :widths: 15 8 40 18 19
   :header-rows: 1

   * - LSIS ID
     - Sec.
     - Requirement
     - Disposition
     - Evidence
   * - LSIS-130
     - 2.3.2
     - Signal generation formula S_AFS(t) = I cos - Q sin
     - Implemented
     - ``modulator.hpp`` [LSIS-AFS-201]
   * - LSIS-140
     - 2.3.2
     - Chip rates I=1.023, Q=5.115 Mchip/s; symbol rate 500 sps
     - Implemented
     - ``modulator.hpp``, ``iq_mux.cpp``
   * - LSIS-150
     - 2.3.3
     - Logic levels: 1 |rarr| -1.0, 0 |rarr| +1.0 (Table 8)
     - Implemented
     - ``modulator.hpp`` branchless BPSK [LSIS-AFS-201]

Section 2.3.4 --- Transmitted Signals Coherency
-------------------------------------------------

.. list-table::
   :widths: 15 8 40 18 19
   :header-rows: 1

   * - LSIS ID
     - Sec.
     - Requirement
     - Disposition
     - Evidence
   * - LSIS-160
     - 2.3.4
     - Primary code / data coherency (|le| 0.5 ns)
     - Implemented
     - ``modulator.hpp`` chip-aligned modulation
   * - LSIS-170
     - 2.3.4
     - Secondary code / primary code coherency (|le| 0.5 ns)
     - Implemented
     - ``matched_code.cpp`` [LSIS-AFS-401]
   * - LSIS-171
     - 2.3.4
     - Tertiary code / secondary code coherency (|le| 0.5 ns)
     - Implemented
     - ``matched_code.cpp`` [LSIS-AFS-401]
   * - LSIS-172
     - 2.3.4
     - Tiered code coherent generation (no chip slips)
     - Implemented
     - ``matched_code.cpp`` [LSIS-AFS-401]
   * - LSIS-180
     - 2.3.4
     - Code-code (data/pilot) coherency < 0.5 ns
     - Implemented
     - ``iq_mux.cpp`` synchronized generation
   * - LSIS-190
     - 2.3.4
     - Code/carrier phase coherency < 0.13 ns (1-sigma, 8 h)
     - RF / Out of Scope
     - Transmitter phase center; no hardware

Section 2.3.5 --- Spreading Codes
-----------------------------------

.. list-table::
   :widths: 15 8 40 18 19
   :header-rows: 1

   * - LSIS ID
     - Sec.
     - Requirement
     - Disposition
     - Evidence
   * - LSIS-200
     - 2.3.5.1
     - Code lengths per Table 9 (I: 2046, Q: 10230/4/1500)
     - Implemented
     - ``gold.hpp``, ``weil.hpp``, ``prn_codes.hpp`` [LSIS-AFS-101/102]
   * - LSIS-210
     - 2.3.5.1
     - Data/pilot code synchronicity (Figure 3)
     - Implemented
     - ``iq_mux.cpp`` synchronized epochs
   * - LSIS-220
     - 2.3.5.1
     - Tertiary code synchronized with frame start
     - Implemented
     - ``matched_code.cpp`` frame-aligned tertiary
   * - LSIS-221
     - 2.3.5.3
     - AFS-I primary code: 2047 Gold, short-cycled to 2046 (Appendix C)
     - Implemented
     - ``gold.hpp`` [LSIS-AFS-101]
   * - LSIS-222
     - 2.3.5.3
     - AFS-Q primary code: Weil length 10230 (Appendix D)
     - Implemented
     - ``weil.hpp`` [LSIS-AFS-102]
   * - LSIS-223
     - 2.3.5.3
     - AFS-Q secondary code: 4 sequences per Table 10
     - Implemented
     - ``matched_code.cpp`` [LSIS-AFS-401]
   * - LSIS-224
     - 2.3.5.3
     - AFS-Q tertiary code: Weil length 1500 (Appendix E)
     - Implemented
     - ``weil.hpp`` [LSIS-AFS-102]
   * - LSIS-260
     - 2.3.5.4
     - Matched-code assignment per LNSP node (Table F-1)
     - TBC in Spec
     - {LSIS-TBD-2001}; interim Table 11 codes implemented

Section 2.4 --- Navigation Message Format
-------------------------------------------

.. list-table::
   :widths: 15 8 40 18 19
   :header-rows: 1

   * - LSIS ID
     - Sec.
     - Requirement
     - Disposition
     - Evidence
   * - LSIS-300
     - 2.4.1
     - Spare bits: alternating 0/1 starting with 0 on MSB
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-300]
   * - LSIS-310
     - 2.4.1
     - Frame structure: SP + SB1 + other subframes
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-601]
   * - LSIS-320
     - 2.4.1
     - Bit/byte ordering: MSB first (Figure 6)
     - Implemented
     - ``message_parser.cpp`` [LSIS-AFS-320]
   * - LSIS-330
     - 2.4.1
     - Sync pattern: 68 symbols, 0xCC63F74536F49E04A
     - Implemented
     - ``frame.hpp`` [LSIS-AFS-601]
   * - LSIS-340
     - 2.4.1
     - SP shall not be encoded
     - Implemented
     - ``frame.cpp`` SP prepended after encoding
   * - LSIS-410
     - 2.4.2
     - SB1 data per Table 13 (FID 2 bits, TOI 7 bits)
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-601]
   * - LSIS-415
     - 2.4.2
     - SB1 bit assignment: FID=2 MSBs, TOI=7 LSBs
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-602]
   * - LSIS-420
     - 2.4.3
     - Frame structure identification via FID (Table 15)
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-603]
   * - LSIS-FID0-430
     - 2.4.3.1
     - FID=0 structure per Figure 9
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-604]
   * - LSIS-FID0-440
     - 2.4.3.1
     - SB2 encoding: 1200 bits, LDPC rate 1/2
     - Implemented
     - ``ldpc.hpp`` [LSIS-AFS-320]
   * - LSIS-FID0-450
     - 2.4.3.1
     - SB3 encoding: 870 bits, LDPC rate 1/2
     - Implemented
     - ``ldpc.hpp`` [LSIS-AFS-320]
   * - LSIS-FID0-460
     - 2.4.3.1
     - SB4 encoding: 870 bits, LDPC rate 1/2
     - Implemented
     - ``ldpc.hpp`` [LSIS-AFS-320]
   * - LSIS-FID0-465
     - 2.4.3.1
     - CRC guards all info in SB2/SB3/SB4 incl. spare bits
     - Implemented
     - ``crc.hpp`` [LSIS-AFS-467]
   * - LSIS-FID0-467
     - 2.4.3.1
     - CRC-24 generator polynomial G(X) = (1+X)*P(X)
     - Implemented
     - ``crc.hpp`` CRC-24Q [LSIS-AFS-467]
   * - LSIS-FID0-469
     - 2.4.3.1
     - CRC computation: 24 parity bits p_i
     - Implemented
     - ``crc.hpp`` [LSIS-AFS-467]
   * - LSIS-FID0-470
     - 2.4.3.1
     - Interleaving: 60 |times| 98, 5880 symbols (Table 17)
     - Implemented
     - ``interleaver.hpp`` [LSIS-AFS-470/471]
   * - LSIS-FID0-500
     - 2.4.3.1
     - SB2 broadcast in every frame
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-605]
   * - LSIS-FID0-510
     - 2.4.3.1
     - SB2 structure identical in every frame
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-605]
   * - LSIS-FID0-520
     - 2.4.3.1
     - SB2 contains CED, ToT (WN/ITOW), Health, TimeConv, CRC
     - Implemented
     - ``message_parser.cpp`` WN/ITOW extraction; payload TBW
   * - LSIS-FID0-530
     - 2.4.3.1
     - SB2 layout per Table 18 (1176 data + 24 CRC = 1200)
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-605]
   * - LSIS-FID0-540
     - 2.4.3.1
     - SB2 LNSP-specific data (FLEX)
     - FLEX
     - LNSP implementation flexibility
   * - LSIS-FID0-550
     - 2.4.3.1
     - SB2 mandatory spare bits {LSIS-TBC-2022}
     - TBC in Spec
     - Implemented with alternating pattern per LSIS-300
   * - LSIS-FID0-560
     - 2.4.3.1
     - SB3 layout per Table 19 (type + data + CRC = 870)
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-606]
   * - LSIS-FID0-570
     - 2.4.3.1
     - SB3 content identified by type field
     - Implemented
     - ``message_parser.cpp`` [LSIS-AFS-801]
   * - LSIS-FID0-580
     - 2.4.3.1
     - SB3 fundamental data: MOrbit Almanac (MSG-G5)
     - TBW in Spec
     - MSG-G5 content is {LSIS-TBW-2007}
   * - LSIS-FID0-590
     - 2.4.3.1
     - SB3 optional data: MSG-G3/G10/G11/G23/G24/G31/G32
     - TBW in Spec
     - Most message contents are TBW
   * - LSIS-FID0-600
     - 2.4.3.1
     - SB4 layout per Table 20 (type + data + CRC = 870)
     - Implemented
     - ``frame.cpp`` [LSIS-AFS-606]
   * - LSIS-FID0-610
     - 2.4.3.1
     - SB4 content identified by type field
     - Implemented
     - ``message_parser.cpp`` [LSIS-AFS-610]
   * - LSIS-FID0-620
     - 2.4.3.1
     - SB4 fundamental data: MSG-G1
     - TBW in Spec
     - MSG-G1 content is {LSIS-TBW-2004}
   * - LSIS-FID0-630
     - 2.4.3.1
     - SB4 optional data: MSG-G3/G5/G10/G11/G17/G23/G28/G31/G32
     - TBW in Spec
     - Most message contents are TBW

Section 2.5 --- Messages and Data Content
-------------------------------------------

.. list-table::
   :widths: 15 8 40 18 19
   :header-rows: 1

   * - LSIS ID
     - Sec.
     - Requirement
     - Disposition
     - Evidence
   * - LSIS-720
     - 2.5.5
     - ToT fields definition (WN, ITOW, TOI per Table 22)
     - Implemented
     - ``time_of_transmission.cpp`` [LSIS-AFS-810]
   * - LSIS-730
     - 2.5.5
     - ToT binary representation per Table 22
     - Implemented
     - ``time_of_transmission.cpp`` [LSIS-AFS-810]

Messages with TBW content (no shall-statements to implement):

- MSG-G1 (2.5.1): {LSIS-TBW-2004}
- MSG-G2 (2.5.2): {LSIS-TBW-2005}
- MSG-G4 (2.5.3): {LSIS-TBW-2006}
- MSG-G5 (2.5.4): {LSIS-TBW-2007}
- MSG-G10 (2.5.6): {LSIS-TBW-2008}
- MSG-G11 (2.5.7): {LSIS-TBW-2009}
- MSG-G17 (2.5.10): {LSIS-TBW-2010}
- MSG-G28 (2.5.18): {LSIS-TBW-2011}
- MSG-G30 (2.5.20): {LSIS-TBW-2012}
- MSG-G31 (2.5.21): {LSIS-TBW-2013}
- MSG-G32 (2.5.22): {LSIS-TBW-2014}

Messages that are **not LunaNet 1.0** (placeholders only):

- MSG-G14, MSG-G15, MSG-S19, MSG-G22, MSG-G23, MSG-G24, MSG-G25, MSG-G27, MSG-G29

Appendices
----------

.. list-table::
   :widths: 15 8 40 18 19
   :header-rows: 1

   * - LSIS ID
     - Sec.
     - Requirement
     - Disposition
     - Evidence
   * - Appendix C
     - App. C
     - Gold code initializations for PRN 1--210
     - Implemented
     - ``prn_table.cpp`` all 210 PRNs validated
   * - Appendix D
     - App. D
     - Weil code index k and insertion index p for PRN 1--210
     - Implemented
     - ``prn_table_weil10230.cpp`` all 210 PRNs validated
   * - Appendix E
     - App. E
     - Tertiary Weil code index k for PRN 1--210
     - Implemented
     - ``prn_table_weil1500.cpp`` all 210 PRNs validated
   * - Appendix F
     - App. F
     - Code assignments per LNSP node
     - TBC in Spec
     - {LSIS-TBD-2001}; interim Table 11 used
   * - Appendix G
     - App. G
     - List of algorithms
     - TBW in Spec
     - {LSIS-TBW-2015}
   * - Appendix H
     - App. H
     - CRC-24 definition (CRC-24Q polynomial)
     - Implemented
     - ``crc.hpp`` [LSIS-AFS-467]
   * - Appendix I
     - App. I
     - Reference AFS link budget
     - TBW in Spec
     - {LSIS-TBW-2016}

----

.. |le| unicode:: U+2264
.. |rarr| unicode:: U+2192
.. |times| unicode:: U+00D7
