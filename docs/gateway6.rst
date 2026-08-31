Gateway 6: Message Parsing --- Verification Guide
==================================================

This document maps each Gateway 6 deliverable to its implementation,
tests, and evidence.  A judge can verify compliance by reading this
document top-to-bottom and running the commands in each section.

Quick Build & Run
-----------------

.. code-block:: bash

   task test-cpp              # C++ unit tests + E2E (< 10 s)
   task test-py               # Python wrapper tests (< 2 s)

   # Interoperability test vectors
   python scripts/generate_gateway6_vectors.py
   # Output: build/test_vectors/gateway6_vectors.json (50 vectors)

All tests must pass with exit code 0.

----

Data Flow
---------

Gateway 6 sits at the top of the receiver pipeline, consuming the raw
bit arrays produced by ``frame_decode()`` (Gateway 5) and producing
structured navigation data:

.. code-block:: text

   I/Q samples
     |
     v
   frame_decode() [Gateway 5]
     |
     +---> bch.fid, bch.toi           (SB1: FID + TOI)
     +---> sb2_data[0..1176)          (SB2: raw bits)
     +---> sb3_data[0..846)           (SB3: raw bits)
     +---> sb4_data[0..846)           (SB4: raw bits)
     |
     v
   parse_sb2(sb2_data) [Gateway 6]   ---> wn (13 bits), itow (9 bits)
   parse_sb3(sb3_data)               ---> message type + payload offset
   parse_sb4(sb4_data)               ---> message type + payload offset
     |
     v
   compute_tot(wn, itow, toi) -------> t_F seconds since LRT epoch

----

.. _g6-interop-risk:

Interoperability Risks
----------------------

.. warning::

   **Type Field Width (TBC)**

   The SB3/SB4 type field width is marked ``{LSIS-TBC-2023}`` /
   ``{LSIS-TBC-2024}`` in LSIS V1.0: Tables 19 and 20 say "4 or 6" bits.

   LunaLink uses **6 bits** because Table 21 defines MSG IDs up to 32
   (requires 6 bits to encode).

   **Impact if another implementation uses 4 bits:** message types > 15
   will be unrepresentable, and all subsequent field offsets will differ
   by 2 bits --- interoperability breaks at the type field boundary.

   Teams **must** agree on type field width before cross-decoding.

.. warning::

   **SB2 Field Layout (TBW)**

   The exact bit positions of WN and ITOW within SB2 are **not specified**
   in LSIS V1.0.  Table 22 defines the field widths (13 and 9 bits) but not
   their offsets.

   LunaLink places WN at bit offset 0 and ITOW at bit offset 13, MSB-first
   per LSIS-320.  This is a reasonable default but **not normative**.

   Interoperability requires agreement on SB2 field layout once the spec
   committee defines it.

Both issues are documented as **Spec Finding SF-005**.

----

GD6.1 --- Message Parsers
--------------------------

Spec reference: LSIS V1.0 |sec|\ 2.4, |sec|\ 2.5, Tables 13--22

.. |sec| unicode:: U+00A7

Implementation files:

-  ``cpp/include/lunalink/messages/message_parser.hpp`` --- public API,
   types, result structs, constants
-  ``cpp/messages/message_parser.cpp`` --- bit extraction, subframe
   parsing, spare bit validation

Architecture
~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Component
     - Description
   * - ``extract_bits()``
     - Generic N-bit extractor (1--32 bits) from a one-bit-per-byte
       array at any offset.  MSB-first per LSIS-320.  Validates every
       bit value (must be 0 or 1).  CFI-guarded (2 milestones).
   * - ``parse_sb2()`` [LSIS-AFS-801]
     - Extracts Week Number (13 bits, offset 0) and Interval Time of
       Week (9 bits, offset 13) from decoded SB2 data per LSIS-720,
       Table 22.  Remaining 1154 data bits are TBW in the spec.
   * - ``parse_sb3()`` [LSIS-AFS-802]
     - Extracts the 6-bit type field from SB3 data (|sec|\ 2.4.3.1.7)
       and maps it to one of 23 LSIS message types (Table 21).
   * - ``parse_sb4()`` [LSIS-AFS-803]
     - Extracts the 6-bit type field from SB4 data (|sec|\ 2.4.3.1.8)
       and maps it to one of 23 LSIS message types (Table 21).
   * - ``validate_spare_bits()``
     - Verifies the alternating 0/1 spare bit pattern per LSIS-300.
       Rejects empty spans as caller errors.
   * - ``type_field_to_message_type()``
     - Pure lookup: maps raw 6-bit type field values to the
       ``MessageType`` enum.  Control-plane function (single-exit
       exemption per |sec|\ Control Flow rule 5).

API
~~~

.. code-block:: cpp

   // Status codes (SEU-hardened, Hamming distance >= 4)
   enum class MessageParseStatus : uint8_t {
       kOk             = 0x5AU,
       kInvalidInput   = 0xA5U,
       kFaultDetected  = 0x99U,
   };

   struct Sb2ParseResult {
       MessageParseStatus status;
       uint16_t wn;           // Week Number (0..8191)
       uint16_t itow;         // Interval Time of Week (0..503)
       uint16_t data_offset;  // Bit offset to remaining TBW data
   };

   struct Sb3ParseResult {  // Sb4ParseResult is identical
       MessageParseStatus status;
       uint8_t     type_raw;     // Raw 6-bit value
       MessageType type;         // Mapped enum (or kUnknown)
       uint16_t    data_offset;  // Bit offset to payload
   };

   [[nodiscard]] MessageParseStatus parse_sb2(
       std::span<const uint8_t> sb2_data,
       Sb2ParseResult& result) noexcept;

   [[nodiscard]] MessageParseStatus parse_sb3(
       std::span<const uint8_t> sb3_data,
       Sb3ParseResult& result) noexcept;

   [[nodiscard]] MessageParseStatus parse_sb4(
       std::span<const uint8_t> sb4_data,
       Sb4ParseResult& result) noexcept;

Message Type Coverage
~~~~~~~~~~~~~~~~~~~~~

The ``MessageType`` enum enumerates all 23 MSG IDs from Table 21 so that
the type router never rejects a valid message.  However, the spec
defines three distinct tiers of completeness:

**Tier 1 --- Fully defined (1 message):**

Only **MSG-G8 (Time of Transmission)** has complete field definitions
with bit widths, value ranges, and formulas (Table 22, LSIS-720).
This is the only message whose content LunaLink can fully parse.

**Tier 2 --- LunaNet 1.0, content TBW (13 messages):**

These are part of LNIS V1.0 but their field layouts are marked
``{LSIS-TBW-*}`` (To Be Written).  LunaLink can route them by type
field but cannot parse their payload until the spec defines it.

.. list-table::
   :header-rows: 1
   :widths: 10 35 10 15 30

   * - MSG ID
     - Title
     - Category
     - Subframe
     - TBW Reference
   * - G1
     - LunaNet Network Access Information
     - F
     - SB4
     - {LSIS-TBW-2004}
   * - G2
     - Health and Safety
     - F
     - SB2
     - {LSIS-TBW-2005}
   * - G3
     - Antenna Properties (Multiple)
     - O
     - SB3/SB4
     - {LSIS-TBD-2002}
   * - G4
     - Clock and Ephemeris Data (CED)
     - F
     - SB2
     - {LSIS-TBW-2006}
   * - G5
     - Orbit Almanac (Multiple)
     - F
     - SB3/SB4
     - {LSIS-TBW-2007}
   * - G10
     - Maneuver
     - O
     - SB3/SB4
     - {LSIS-TBW-2008}
   * - G11
     - Attitude State/Ephemeris (Single)
     - O
     - SB3/SB4
     - {LSIS-TBW-2009}
   * - G17
     - Ancillary Info
     - O
     - SB4
     - {LSIS-TBW-2010}
   * - S20
     - LunaSAR Return Message
     - F
     - SB3
     - {LSIS-TBD-2004} (bit allocation only)
   * - G28
     - User Schedule Notice
     - C
     - SB4
     - {LSIS-TBW-2011}
   * - G30
     - Time Conversions
     - F
     - SB2
     - {LSIS-TBW-2012}
   * - G31
     - Augmentation Differential Corrections
     - O
     - SB3/SB4
     - {LSIS-TBW-2013}
   * - G32
     - Coordinate Frame Conversions
     - F
     - SB3/SB4
     - {LSIS-TBW-2014}

*F = Fundamental (mandatory), O = Optional, C = Communication.*

**Tier 3 --- Not LunaNet 1.0 (9 messages):**

These are placeholders for future LNIS versions.  LunaLink enumerates
them so the type router returns ``kUnknown`` rather than crashing on a
valid-but-future type field:

G14 (Conjunction), G15 (Maplet), S19 (LunaSAR Ack), G22 (Non-SAR Ack),
G23 (GNSS Augmentation), G24 (Detection Alert), G25 (Science),
G27 (UIS Response), G29 (FF Commands).

**Summary: 1 fully parseable + 13 routable (content TBW) + 9 future placeholders = 23 total.**

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - CFI sentinels
     - 2-milestone volatile counter (magic 0x6B6B6B6B) in every
       ``[LSIS-AFS-*]``-tagged function; terminal check before ``kOk``
   * - Pessimistic default
     - ``status`` initialised to ``kFaultDetected``; promoted only if
       CFI terminal check passes
   * - Single-exit accumulator
     - All stages guarded by ``if (ok)``; single ``return`` at end
   * - Secure scrub on failure
     - ``secure_scrub_scalar()`` on ``result.wn``, ``result.itow``,
       ``result.type_raw`` on non-``kOk`` paths
   * - Bit-value validation
     - ``extract_bits()`` rejects any byte > 1 (detects SEU in bit array)
   * - SEU-resistant status codes
     - ``MessageParseStatus``: kOk=0x5A, kInvalidInput=0xA5,
       kFaultDetected=0x99 (Hamming distance |geq| 4)
   * - FIZ-1 annotations
     - All CFI terminal check failures annotated ``// FIZ-1``
       (block-level fault containment)
   * - Critical sections
     - All CFI counter increments inside ``LUNALINK_CRITICAL_ENTER`` /
       ``LUNALINK_CRITICAL_EXIT``

.. |geq| unicode:: U+2265

FMEA (``docs/assurance/fmea.rst``):

-  **FM-069**: SEU corrupts bit array during ``extract_bits()``
-  **FM-070**: SEU corrupts SB3/SB4 type field |rarr| wrong message handler
-  **FM-071**: SEU corrupts WN/ITOW after extraction in ``parse_sb2()``

.. |rarr| unicode:: U+2192

Unit tests
~~~~~~~~~~

Run: ``task test-cpp`` (file: ``cpp/tests/messages/test_message_parser.cpp``,
22 tests)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - extract_bits basic
     - 5-bit extraction at offset 0 returns correct value
   * - extract_bits at offset
     - 10-bit extraction at offset 3
   * - extract_bits single bit
     - 1-bit extraction at various positions
   * - extract_bits 32 bits
     - Full 32-bit extraction (0xDEADBEEF)
   * - extract_bits out of bounds
     - Returns ``kInvalidInput``; output zeroed
   * - extract_bits zero n_bits
     - Returns ``kInvalidInput``
   * - extract_bits invalid bit value
     - Byte value 2 detected; returns ``kInvalidInput``
   * - parse_sb2 extracts WN and ITOW
     - WN=1234, ITOW=456 packed and recovered
   * - parse_sb2 boundary WN=8191 ITOW=503
     - Maximum valid values
   * - parse_sb2 zero values
     - All-zero data produces WN=0, ITOW=0
   * - parse_sb2 wrong size
     - 100-byte input returns ``kInvalidInput``
   * - parse_sb2 data_offset
     - ``data_offset == 22`` (WN + ITOW header)
   * - parse_sb3 extracts type field
     - Type=5 maps to ``kG5``
   * - parse_sb3 all known types
     - All 23 known MSG ID values mapped correctly
   * - parse_sb3 unknown type
     - Type=63 returns ``kOk`` with ``kUnknown``
   * - parse_sb3 wrong size
     - Returns ``kInvalidInput``
   * - parse_sb4 extracts type field
     - Type=1 maps to ``kG1``
   * - parse_sb4 wrong size
     - Returns ``kInvalidInput``
   * - validate_spare_bits correct
     - ``[0,1,0,1,0,1,0,1]`` returns true
   * - validate_spare_bits incorrect
     - Starts with 1 returns false
   * - validate_spare_bits empty rejects
     - Empty span returns false (caller error)
   * - type_field_to_message_type unknown
     - Values 0, 6, 7, 9, 255 all return ``kUnknown``

SWIFI scenarios (planned --- ``docs/assurance/fault_coverage.rst``):

-  **FI-37** (planned): CFI skip injection in ``extract_bits()``
-  **FI-38** (planned): Bit-value injection in ``parse_sb2()``
-  **FI-39** (planned): CFI skip injection in ``parse_sb3()`` / ``parse_sb4()``

----

GD6.2 --- Time of Transmission Calculator
------------------------------------------

Spec reference: LSIS-720, Table 22, Figure 11

Implementation files:

-  ``cpp/include/lunalink/messages/time_of_transmission.hpp`` --- API,
   constants, result type
-  ``cpp/messages/time_of_transmission.cpp`` --- ToT formula with CFI

Formula (LSIS-720)
~~~~~~~~~~~~~~~~~~

.. math::

   t_F = WN \times SECWEEK + ITOW \times BI_d + TOI \times F_d + \Delta t_{LRT}

Where:

-  SECWEEK = 604800 s (seconds per week)
-  BI_d = 1200 s (block interval duration = 12 |times| 100 = 20 min)
-  F_d = 12 s (frame duration)
-  |Delta|\ t_LRT = Node Time offset w.r.t. LRT (from MSG-G4, TBW)

.. |times| unicode:: U+00D7
.. |Delta| unicode:: U+0394

API
~~~

.. code-block:: cpp

   enum class TotStatus : uint8_t {
       kOk             = 0x5AU,
       kInvalidToi     = 0xA5U,
       kInvalidItow    = 0x33U,
       kInvalidWn      = 0xCCU,
       kFaultDetected  = 0x99U,
   };

   struct TotResult {
       TotStatus status;
       double    t_f;     // seconds since LRT epoch
   };

   [[nodiscard]] TotResult compute_tot(
       uint16_t wn, uint16_t itow, uint8_t toi,
       double delta_t_lrt = 0.0) noexcept;

Constants
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Constant
     - Value
     - Source
   * - ``kSecWeek``
     - 604800
     - Seconds per week
   * - ``kBlockIntervalDuration``
     - 1200
     - BI_d = 12 |times| 100 (LSIS-720)
   * - ``kFrameDurationSec``
     - 12
     - F_d (LSIS-720)
   * - ``kTotToiMax``
     - 99
     - 7 bits, 0..99 (Table 22)
   * - ``kItowMax``
     - 503
     - 9 bits, 0..503 (Table 22)
   * - ``kWnMax``
     - 8191
     - 13 bits, 0..8191 (Table 22)

Hardening
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - CFI sentinels
     - 2-milestone volatile counter (magic 0x4D4D4D4D): inputs
       validated, formula computed; terminal check before ``kOk``
   * - Pessimistic default
     - ``status`` initialised to ``kFaultDetected``
   * - Range validation
     - WN |leq| 8191, ITOW |leq| 503, TOI |leq| 99; specific error
       codes for each violation
   * - Single-exit accumulator
     - All validation stages guarded by ``if (ok)``; single ``return``
   * - Secure scrub on failure
     - ``secure_scrub_scalar(result.t_f)`` on non-``kOk`` paths
   * - Overflow prevention
     - ``double`` arithmetic avoids ``uint32_t`` overflow at WN=8191
       (|approx|\ 4.95 |times| 10\ :sup:`9`)
   * - FIZ-1 annotation
     - CFI terminal check failure annotated ``// FIZ-1``

.. |leq| unicode:: U+2264
.. |approx| unicode:: U+2248

FMEA: **FM-072** --- ``compute_tot()`` arithmetic overflow or
SEU-corrupted input.

Unit tests
~~~~~~~~~~

Run: ``task test-cpp`` (file: ``cpp/tests/messages/test_time_of_transmission.cpp``,
12 tests)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - All zeros
     - WN=0, ITOW=0, TOI=0 |rarr| t_F = 0.0
   * - Basic formula
     - WN=1, ITOW=2, TOI=5 |rarr| t_F = 607260.0
   * - With delta
     - |Delta|\ t_LRT = 1.5 |rarr| t_F = 1.5
   * - Boundary max
     - WN=8191, ITOW=503, TOI=99 |rarr| t_F = 4954521588.0
   * - Negative delta
     - |Delta|\ t_LRT = |minus|\ 0.5 |rarr| t_F = 604799.5
   * - Individual WN
     - WN=1, rest=0 |rarr| t_F = 604800.0 (one week)
   * - Individual ITOW
     - ITOW=1, rest=0 |rarr| t_F = 1200.0 (one BI)
   * - Individual TOI
     - TOI=1, rest=0 |rarr| t_F = 12.0 (one frame)
   * - Invalid WN
     - WN=8192 |rarr| ``kInvalidWn``
   * - Invalid ITOW
     - ITOW=504 |rarr| ``kInvalidItow``
   * - Invalid TOI
     - TOI=100 |rarr| ``kInvalidToi``
   * - Week consistency
     - Last frame of week + 12 s = first frame of next week

.. |minus| unicode:: U+2212

----

E2E Round-Trip Test
-------------------

Run: ``./build/test/test_lunalink "[e2e]" "~[channel]"``
(file: ``cpp/tests/integration/test_e2e.cpp``)

.. code-block:: text

   TX: Pack WN=100, ITOW=42, TOI=17, SB3 type=G5, SB4 type=G1
     |
     v
   frame_build()  -->  6000-symbol frame
     |
     v
   binary_to_bipolar()  -->  soft symbols (clean channel)
     |
     v
   frame_decode()  -->  sb2_data, sb3_data, sb4_data
     |
     v
   parse_sb2()  -->  wn=100, itow=42  ✓
   parse_sb3()  -->  type=G5          ✓
   parse_sb4()  -->  type=G1          ✓
     |
     v
   compute_tot(100, 42, 17)  -->  t_F = 60530604.0  ✓

This test verifies the **complete pipeline** from packed navigation data
through TX, RX decode, message parsing, and time reconstruction.

----

Python Bindings & Tests
-----------------------

All C++ types and functions are exposed via nanobind:

.. code-block:: python

   from lunalink.afs import (
       parse_sb2, parse_sb3, parse_sb4, compute_tot,
       MessageType, MessageParseStatus, TotStatus,
       Sb2ParseResult, Sb3ParseResult, Sb4ParseResult, TotResult,
       WN_MAX, ITOW_MAX, TOT_TOI_MAX, SEC_WEEK,
       BLOCK_INTERVAL_DURATION, FRAME_DURATION_SEC,
   )

   # Parse decoded SB2 data
   result = parse_sb2(sb2_data)
   print(f"WN={result.wn}, ITOW={result.itow}")

   # Parse SB3 type field
   result = parse_sb3(sb3_data)
   print(f"Message type: {result.type}")  # e.g., MessageType.G5

   # Compute Time of Transmission
   tot = compute_tot(wn=100, itow=42, toi=17)
   print(f"t_F = {tot.t_f} seconds since LRT epoch")

Type stubs: ``src/lunalink/afs/_afs.pyi``

Python tests: ``tests/test_afs.py`` --- classes ``TestParseSb2`` (5 tests),
``TestParseSb3`` (3 tests), ``TestParseSb4`` (2 tests), ``TestComputeTot``
(9 tests including full E2E round-trip)

Run: ``task test-py`` (92 tests, 100% Python coverage)

----

Interoperability Test Vectors
-----------------------------

LunaLink provides **50 machine-readable test vectors** for cross-team
validation:

.. code-block:: bash

   python scripts/generate_gateway6_vectors.py
   # Output: build/test_vectors/gateway6_vectors.json

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Section
     - Count
     - Contents
   * - ``time_of_transmission``
     - 13
     - ToT formula inputs/outputs; boundary values (WN=8191, ITOW=503,
       TOI=99); invalid inputs; negative delta; week rollover pair
   * - ``sb2_parse``
     - 6
     - WN + ITOW extraction with packed first-22-bits arrays; zero,
       mid-range, and boundary values
   * - ``type_field_routing``
     - 28
     - All 23 known MSG types + 5 unknown values (0, 6, 7, 9, 63);
       bit-level type field encoding
   * - ``spare_bits``
     - 3
     - LSIS-300 alternating pattern: correct, incorrect, single-bit

Each vector includes ``spec_reference`` and ``interop_notes`` fields.
The JSON header includes a prominent ``interop_notes`` block documenting
the type field width TBC and SB2 layout assumptions.

----

Spec Finding SF-005
-------------------

**Title:** Message field layouts are TBW in LSIS V1.0

**Severity:** Interoperability risk (High)

**Description:** Of the 23 message types in Table 21, only **1** (MSG-G8)
has fully defined field layouts.  13 messages are part of LunaNet 1.0
but have content marked ``{LSIS-TBW-*}``.  8 messages are future
placeholders not included in LunaNet 1.0.  This means the message
parsing gateway can only fully parse Time of Transmission data; all
other message payloads are opaque until the spec defines them.

**Impact on interoperability:**

1. The bit positions of WN and ITOW within SB2 are not specified.
   LunaLink assumes offset 0 (WN first, then ITOW), MSB-first per
   LSIS-320.  Other implementations may differ.
2. The SB3/SB4 type field width is TBC (4 or 6 bits).  LunaLink uses
   6 bits.  A 4-bit implementation cannot represent MSG IDs > 15.
3. No message content can be parsed beyond the type field until the
   spec defines the field layouts.

**Recommendation to LNIS working group:** Prioritise the SB2 field
layout definition (MSG-G4, MSG-G2, MSG-G8 field ordering and offsets)
and resolve the type field width TBC in the next LSIS revision.

----

Compliance Cross-Reference
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 16 48 36

   * - ID
     - Requirement
     - Evidence
   * - LSIS-AFS-801
     - Parse SB2: extract WN (13b) and ITOW (9b)
     - ``message_parser.cpp`` + ``test_message_parser.cpp`` (4 tests)
   * - LSIS-AFS-802
     - Parse SB3: extract 6-bit type field, map to Table 21
     - ``message_parser.cpp`` + ``test_message_parser.cpp`` (4 tests)
   * - LSIS-AFS-803
     - Parse SB4: extract 6-bit type field, map to Table 21
     - ``message_parser.cpp`` + ``test_message_parser.cpp`` (2 tests)
   * - LSIS-AFS-810
     - Compute ToT per LSIS-720 formula
     - ``time_of_transmission.cpp`` + ``test_time_of_transmission.cpp`` (12 tests)
   * - LSIS-300
     - Spare bits: alternating 0/1 starting with 0
     - ``validate_spare_bits()`` + 3 tests
   * - LSIS-320
     - Bit ordering: MSB first, MSB transmitted first
     - ``extract_bits()`` + 7 tests
   * - LSIS-720
     - ToT field definitions (WN, ITOW, TOI widths and ranges)
     - Constants in ``time_of_transmission.hpp``; range validation tests

----

Test Summary
------------

.. list-table::
   :header-rows: 1
   :widths: 35 10 15 15

   * - Suite
     - Tests
     - Tags
     - How to run
   * - GD6.1 Message parser unit tests
     - 22
     - ``[messages]``
     - ``task test-cpp``
   * - GD6.2 ToT calculator unit tests
     - 12
     - ``[messages]``
     - ``task test-cpp``
   * - E2E: TX |rarr| RX |rarr| parse |rarr| ToT
     - 1
     - ``[e2e][rx]``
     - ``task test-cpp``
   * - Python wrapper tests
     - 19
     - ---
     - ``task test-py``
   * - SWIFI (Gateway 6: FI-37 to FI-40)
     - 0 (planned)
     - ``[swifi]``
     - ``task swifi``

**Total: 54 tests** (34 C++ unit + 1 C++ E2E + 19 Python)

All tests pass with exit code 0.

----

Related Documents
-----------------

-  :doc:`signal/compliance_matrix` --- LSIS-AFS-801 through 810
-  :doc:`signal/architecture` --- module-to-gateway mapping
-  :doc:`signal/icd` --- formal interface control document
-  :doc:`assurance/mission_assurance` --- CFI, scrubbing patterns
-  :doc:`assurance/c_plus_plus_guidelines` --- coding standard
-  :doc:`assurance/fmea` --- FM-069 to FM-072 (Gateway 6)
-  :doc:`assurance/fault_coverage` --- FI-37 to FI-40 (planned)
