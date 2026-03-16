Gateway 3: Navigation Message Framing --- Verification Guide
=============================================================

This document maps each Gateway 3 deliverable to its implementation,
tests, and evidence.  A judge can verify compliance by reading this
document top-to-bottom and running the commands in each section.

Quick Build & Run
-----------------

.. code-block:: bash

   task test-cpp              # builds and runs all unit tests (< 2 s)
   task test-py               # runs Python binding tests
   task swifi                 # runs SWIFI fault-injection suite

All tests must pass with exit code 0.

----

GD3.1 --- Synchronisation Pattern (SP)
---------------------------------------

Spec references: LSIS-330 (Table 12), LSIS-340

The synchronisation pattern is 68 uncoded binary symbols transmitted at
the start of every frame.  It enables receiver frame boundary detection
via its favourable autocorrelation properties.

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Parameter
     - Value
   * - Length
     - 68 symbols
   * - Hex pattern
     - ``CC63F74536F49E04A`` (Table 12)
   * - Encoding
     - None (LSIS-340: uncoded)

Implementation: ``cpp/signal/frame.cpp`` --- ``kSyncPattern`` array,
``constinit``, ``alignas(64)``, placed in ``LUNALINK_LUT_SECTION``.

----

GD3.2 --- Subframe 1 Builder (SB1)
------------------------------------

Spec references: LSIS-410, LSIS-415 (Table 13, Table 14), BCH(51,8)

SB1 carries the Frame Identifier (FID) and Time of Interval (TOI) in a
9-bit field, channel-encoded with a Bose--Chaudhuri--Hocquenghem code to
produce 52 binary symbols.

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Field
     - Bits
     - Position
     - Description
   * - FID
     - 2
     - MSBs (bits 0--1)
     - Frame structure identifier (0 = SB2+SB3+SB4; 1--3 reserved)
   * - TOI
     - 7
     - LSBs (bits 2--8)
     - Time of Interval [0, 99], leading edge of next 12 s frame

BCH encoding
~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Parameter
     - Value
   * - Code
     - BCH(51, 8)
   * - Generator polynomial
     - 763 (octal) = 1+X\ :sup:`3`\ +X\ :sup:`4`\ +X\ :sup:`5`\ +X\ :sup:`6`\ +X\ :sup:`7`\ +X\ :sup:`8`
   * - MSB prepend
     - Bit 0 of SB1 (FID MSB) modulo-2 added and prepended |rarr| 52 symbols

.. |rarr| unicode:: U+2192

Implementation files:

-  ``cpp/include/lunalink/signal/bch.hpp`` --- ``Fid``, ``Toi`` (TMR-hardened),
   ``bch_encode()``, ``bch_decode()``
-  ``cpp/signal/bch.cpp`` --- encoder/decoder, codebook LUT

Spec test vector (Figure 8): FID=0, TOI=69 (0x045) |rarr| encoded = 0x229f61dbb84a0.
Verified in ``test_frame.cpp``: ``frame_build_partial SB1 with spec test vector``.

----

GD3.3 --- Subframe 2 Builder (SB2)
------------------------------------

Spec references: LSIS-FID0-440, LSIS-FID0-465, LSIS-FID0-467,
LSIS-FID0-510, LSIS-FID0-530 (Table 18)

SB2 carries clock, ephemeris, health, and time conversion data.  Its
structure is identical in every frame (LSIS-FID0-510).

Bit allocation (Table 18)
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - Field
     - Bits
     - Value
     - Notes
   * - Data
     - 1176
     - ``kSb2DataBits``
     - CED, ToT, Health, Time Conversions, spare
   * - CRC-24Q
     - 24
     - ``kCrc24Bits``
     - Polynomial 0x864CFB (LSIS-FID0-467)
   * - **Total (LDPC input)**
     - **1200**
     - ``kSb2MsgBits``
     - Rate-1/2 LDPC (Annex 1 §2.1 matrices)
   * - **Encoded output**
     - **2400**
     - ``kSb2Length``
     - Punctured: first z=240 systematic bits removed

Pipeline: ``subframe_build_sb2()`` |rarr| CRC-24Q append |rarr| LDPC SF2 encode |rarr| 2400 symbols.

Implementation files:

-  ``cpp/include/lunalink/signal/frame.hpp`` --- API, constants
-  ``cpp/signal/frame.cpp`` --- ``subframe_build_sb2()`` via ``subframe_build_ldpc()``
-  ``cpp/include/lunalink/signal/crc.hpp`` --- ``crc24_calculate()``
-  ``cpp/include/lunalink/signal/ldpc.hpp`` --- ``ldpc_encode()``

----

GD3.4 --- Subframe 3 Builder (SB3)
------------------------------------

Spec references: LSIS-FID0-450, LSIS-FID0-560 (Table 19), LSIS-FID0-570

SB3 is a dynamic subframe carrying variable content identified by a type
field.  Fundamental content: Orbit Almanac (MSG-G5).

Bit allocation (Table 19)
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - Field
     - Bits
     - Value
     - Notes
   * - SB3 type + Data
     - 846
     - ``kSb3DataBits``
     - Type field (4 or 6 bits, {LSIS-TBC-2023}) + message data
   * - CRC-24Q
     - 24
     - ``kCrc24Bits``
     - Guards all information including spare bits (LSIS-FID0-465)
   * - **Total (LDPC input)**
     - **870**
     - ``kSb3MsgBits``
     - + 10 zero filler bits |rarr| 880 (LDPC SF3 input)
   * - **Encoded output**
     - **1740**
     - ``kSb3Length``
     - Punctured: first z=176 systematic + filler bits removed

Filler bits (§2.4.3.1.2): 10 zero bits appended to the 870-bit message
before LDPC encoding.  In the encoder, ``K_nominal=870`` and ``K_full=880``;
the workspace array ``s[]`` is zero-initialised, so positions 870--879
are implicitly zero.  Both the first z=176 systematic bits and the filler
bits are excluded from the transmitted output (punctured).

Pipeline: ``subframe_build_sb3()`` |rarr| CRC-24Q append |rarr| LDPC SF3 encode |rarr| 1740 symbols.

----

GD3.5 --- Subframe 4 Builder (SB4)
------------------------------------

Spec references: LSIS-FID0-460, LSIS-FID0-600 (Table 20), LSIS-FID0-610

SB4 is a dynamic subframe carrying network access information (MSG-G1)
and optional content identified by a type field.

Bit allocation (Table 20)
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - Field
     - Bits
     - Value
     - Notes
   * - SB4 type + Data
     - 846
     - ``kSb4DataBits``
     - Type field (4 or 6 bits, {LSIS-TBC-2024}) + message data
   * - CRC-24Q
     - 24
     - ``kCrc24Bits``
     - Guards all information including spare bits
   * - **Total (LDPC input)**
     - **870**
     - ``kSb4MsgBits``
     - + 10 zero filler bits |rarr| 880 (reuses SF3 matrices)
   * - **Encoded output**
     - **1740**
     - ``kSb4Length``
     - Punctured: first z=176 systematic + filler bits removed

SB4 reuses the SF3 LDPC matrices (Annex 1 §2.2).  In the implementation,
``LdpcSubframe::kSF4`` maps to the SF3 matrix set.

Pipeline: ``subframe_build_sb4()`` |rarr| CRC-24Q append |rarr| LDPC SF3 encode |rarr| 1740 symbols.

----

GD3.6 --- Frame Assembler
--------------------------

Spec references: LSIS-310, LSIS-FID0-430 (Figure 9), LSIS-FID0-470 (Table 17)

The frame assembler constructs a complete 6000-symbol AFS navigation frame
from SP, SB1, SB2, SB3, and SB4.

Frame structure (Figure 9)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 45

   * - Component
     - Symbols
     - Constant
     - Description
   * - Sync Pattern
     - 68
     - ``kSyncLength``
     - Uncoded (LSIS-340)
   * - SB1
     - 52
     - ``kSb1Length``
     - BCH(51,8)-encoded FID + TOI
   * - Payload (SB2+SB3+SB4)
     - 5880
     - ``kPayloadLength``
     - 60 |x| 98 block-interleaved
   * - **Total**
     - **6000**
     - ``kFrameLength``
     - 12.0 s at 500 sps

.. |x| unicode:: U+00D7

Compile-time assertion: ``static_assert(kSyncLength + kSb1Length + kPayloadLength == kFrameLength)``
and ``static_assert(kSb2Length + kSb3Length + kSb4Length == kPayloadLength)`` (in ``test_frame.cpp``).

Assembly pipeline
~~~~~~~~~~~~~~~~~

``frame_build()`` executes a 6-milestone CFI-hardened pipeline:

1. **Sync pattern** --- copy 68-symbol ``kSyncPattern`` to frame[0..67]
2. **SB1 header** --- ``bch_encode(fid, toi)`` into frame[68..119]
3. **SB2 build** --- ``subframe_build_ldpc(SF2, ...)`` |rarr| staging[0..2399]
4. **SB3 build** --- ``subframe_build_ldpc(SF3, ...)`` |rarr| staging[2400..4139]
5. **SB4 build** --- ``subframe_build_ldpc(SF4, ...)`` |rarr| staging[4140..5879]
6. **Block interleave** --- ``block_interleave(staging)`` |rarr| frame[120..5999]

Each milestone increments a volatile CFI counter.  Terminal check verifies
``cfi_count == kCfiMagicFrame + 6`` before returning ``kOk``.

Block interleaver (Table 17)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Parameter
     - Value
   * - Block size
     - 5880 symbols (SB2 + SB3 + SB4)
   * - Dimensions
     - 60 rows |x| 98 columns
   * - Write order
     - Row-wise (left to right, top to bottom)
   * - Read order
     - Column-wise (top to bottom, left to right)

Implementation: ``cpp/signal/interleaver.cpp`` with CFI sentinels and
mirrored calculation.

API
~~~

.. code-block:: cpp

   // Build complete 6000-symbol frame
   [[nodiscard]] FrameStatus frame_build(
       Fid fid, Toi toi,
       std::span<const uint8_t, 1176> sb2_data,
       std::span<const uint8_t, 846>  sb3_data,
       std::span<const uint8_t, 846>  sb4_data,
       std::span<uint8_t, 6000>       out,
       LdpcEncodeWorkspace&           workspace) noexcept;

   // Individual subframe builders
   [[nodiscard]] FrameStatus subframe_build_sb2(
       std::span<const uint8_t, 1176> data,
       std::span<uint8_t, 2400>       out,
       LdpcEncodeWorkspace&           workspace) noexcept;

   [[nodiscard]] FrameStatus subframe_build_sb3(
       std::span<const uint8_t, 846>  data,
       std::span<uint8_t, 1740>       out,
       LdpcEncodeWorkspace&           workspace) noexcept;

   [[nodiscard]] FrameStatus subframe_build_sb4(
       std::span<const uint8_t, 846>  data,
       std::span<uint8_t, 1740>       out,
       LdpcEncodeWorkspace&           workspace) noexcept;

   // Frame Commit Gate (defence-in-depth CRC-24Q check)
   [[nodiscard]] FrameStatus commit_frame(
       std::span<const uint8_t> frame_buf) noexcept;

----

Hardening
---------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Feature
     - Implementation
   * - Control Flow Integrity
     - Every builder uses volatile CFI counter with milestone tracking;
       terminal check verifies exact milestone count before returning ``kOk``.
       Subframe builders: 3 milestones. Frame assembler: 6 milestones.
   * - SEU-resistant status codes
     - ``FrameStatus`` values use non-contiguous bit patterns with Hamming
       distance |geq| 4 (``kOk`` = 0x5A, ``kFaultDetected`` = 0x99, etc.)
   * - Aliasing protection
     - ``spans_overlap()`` runtime check on all input/output buffer pairs;
       frame assembler checks all three data buffers against output
   * - Secure scrub
     - ``secure_scrub()`` on output buffer on any error path; staging buffer
       always scrubbed after use regardless of success/failure
   * - Zero dynamic allocation
     - All temporaries (staging, msg_buf, CRC) are stack-allocated with
       ``alignas(16)``; ``LdpcEncodeWorkspace`` is caller-provided
   * - Pessimistic default
     - Status initialised to ``kFaultDetected``; only set to ``kOk`` on
       successful CFI terminal check
   * - LDPC self-check
     - Post-encode H |cdot| c = 0 verification detects encoder corruption
   * - Frame Commit Gate
     - ``commit_frame()`` provides a defence-in-depth CRC-24Q check before
       transmit DMA hand-off

.. |geq| unicode:: U+2265
.. |cdot| unicode:: U+00B7

----

Unit Tests
----------

Run: ``task test-cpp`` (file: ``cpp/tests/signal/test_frame.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Test case
     - What it verifies
   * - subframe_build_sb2 returns kOk for zero data
     - Basic SB2 pipeline executes without error
   * - subframe_build_sb3 returns kOk for zero data
     - Basic SB3 pipeline executes without error
   * - subframe_build_sb4 returns kOk for zero data
     - Basic SB4 pipeline executes without error
   * - subframe_build_sb2 output is binary
     - All 2400 encoded symbols |isin| {0, 1}
   * - subframe_build_sb2 round-trip: encode, decode, verify CRC
     - TX |rarr| LDPC decode |rarr| data recovery + CRC-24Q pass
   * - subframe_build_sb3 round-trip: encode, decode, verify CRC
     - TX |rarr| LDPC decode |rarr| data recovery + CRC-24Q pass
   * - subframe_build_sb4 round-trip: encode, decode, verify CRC
     - TX |rarr| LDPC decode |rarr| data recovery + CRC-24Q pass
   * - frame_build returns kOk for valid inputs
     - Full 6000-symbol frame assembles without error
   * - frame_build all symbols are binary
     - All 6000 output symbols |isin| {0, 1}
   * - frame_build SB1 region matches standalone bch_encode
     - BCH encoding consistency between standalone and frame paths
   * - frame_build accepts all valid FID values
     - FID 0--3 all produce valid frames
   * - frame_build symbol count: SP(68) + SB1(52) + payload(5880) = 6000
     - Compile-time ``static_assert`` on frame structure constants
   * - frame_build payload round-trip: deinterleave, decode, verify
     - Full TX |rarr| deinterleave |rarr| LDPC decode |rarr| data recovery
       for all three subframes + SB1 BCH decode

.. |isin| unicode:: U+2208

SWIFI fault injection (``cpp/tests/signal/test_fault_injection.cpp``)

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Scenario
     - What it verifies
   * - FI-13: subframe_build_sb2 LDPC matrix corruption
     - Corrupted LDPC SF2 matrix detected; ``kOk`` not returned;
       output buffer scrubbed to zero
   * - FI-14: frame_build LDPC matrix corruption
     - Corrupted LDPC SF2 matrix in full frame path detected;
       ``kOk`` not returned; output buffer scrubbed to zero

----

Python Bindings
---------------

All GD3 components are exposed to Python via ``lunalink.afs``.

.. list-table::
   :header-rows: 1
   :widths: 25 40 35

   * - Function
     - Signature
     - Notes
   * - ``frame_build_partial``
     - ``(fid: int, toi: int) -> NDArray[uint8]``
     - SP + SB1 only (6000 symbols, payload zeroed)
   * - ``frame_build``
     - ``(fid, toi, sb2_data, sb3_data, sb4_data) -> NDArray[uint8]``
     - Full 6000-symbol frame
   * - ``subframe_build_sb2``
     - ``(data: NDArray[uint8]) -> NDArray[uint8]``
     - 1176 bits |rarr| 2400 encoded symbols
   * - ``subframe_build_sb3``
     - ``(data: NDArray[uint8]) -> NDArray[uint8]``
     - 846 bits |rarr| 1740 encoded symbols
   * - ``subframe_build_sb4``
     - ``(data: NDArray[uint8]) -> NDArray[uint8]``
     - 846 bits |rarr| 1740 encoded symbols

Python tests: ``task test-py`` (file: ``tests/test_afs.py``)

----

Compliance Cross-Reference
---------------------------

.. list-table::
   :header-rows: 1
   :widths: 18 52 30

   * - Spec ID
     - Requirement
     - Evidence
   * - LSIS-310
     - Frame = SP + SB1 + subframes
     - ``frame_build()`` + static_assert
   * - LSIS-330
     - SP = 68 symbols, CC63F74536F49E04A (Table 12)
     - ``kSyncPattern`` array in ``frame.cpp``
   * - LSIS-340
     - SP uncoded
     - Raw copy, no encoding applied
   * - LSIS-410
     - SB1 = FID(2) + TOI(7) = 9 data bits (Table 13)
     - ``Fid`` [0--3], ``Toi`` [0--99]
   * - LSIS-415
     - BCH(51,8) with MSB prepend |rarr| 52 symbols (Table 14)
     - ``bch_encode()`` + spec test vector
   * - LSIS-FID0-430
     - FID=0 frame structure per Figure 9
     - ``frame_build()`` + round-trip test
   * - LSIS-FID0-440
     - SB2: 1200 bits |rarr| LDPC 1/2 |rarr| 2400 symbols
     - ``subframe_build_sb2()`` + ``kSb2*`` constants
   * - LSIS-FID0-450
     - SB3: 870 bits |rarr| LDPC 1/2 |rarr| 1740 symbols
     - ``subframe_build_sb3()`` + ``kSb3*`` constants
   * - LSIS-FID0-460
     - SB4: 870 bits |rarr| LDPC 1/2 |rarr| 1740 symbols
     - ``subframe_build_sb4()`` + ``kSb4*`` constants
   * - LSIS-FID0-465
     - CRC guards all information in SB2, SB3, SB4
     - ``crc24_calculate()`` over full data span
   * - LSIS-FID0-467
     - CRC-24Q polynomial G(X) = (1+X)P(X), 0x864CFB
     - ``crc.hpp`` documents polynomial
   * - LSIS-FID0-470
     - 60 |x| 98 block interleaver, 5880 symbols (Table 17)
     - ``block_interleave()`` + ``kInterleaver*`` constants
   * - §2.4.3.1.2
     - SF3/SF4: 10 zero filler bits appended before LDPC
     - ``K_full=880`` vs ``K_nominal=870`` in ``ldpc_encode()``
   * - §2.4.3.1.2
     - Puncture first z bits (z=240 SF2, z=176 SF3/SF4) + filler
     - Encoder output loop: ``s[z..K_nominal)``

----

Test Summary
------------

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 20 25

   * - Suite
     - Tests
     - Assertions
     - Run time
     - How to run
   * - GD3 unit tests
     - 13
     - ~200
     - < 2 s
     - ``task test-cpp``
   * - SWIFI (GD3 scenarios)
     - 2
     - ~20
     - < 1 s
     - ``task swifi``
   * - Python bindings
     - included in pytest
     - ---
     - < 5 s
     - ``task test-py``

All C++ unit tests pass in under 2 seconds with exit code 0.
SWIFI scenarios verify LDPC matrix corruption detection and output
scrubbing for both standalone subframe and full frame pipelines.

Related documents:

-  :doc:`signal/compliance_matrix` --- LSIS-310, LSIS-330, LSIS-340, LSIS-410--470
-  :doc:`signal/architecture` --- module-to-gateway mapping
-  :doc:`signal/signal_chain` --- transmitter block diagram
-  :doc:`signal/icd` --- formal interface control document
-  :doc:`assurance/mission_assurance` --- CFI, secure scrub, pessimistic default
-  :doc:`assurance/private/fmea` --- frame builder failure modes (FM-005)
-  :doc:`assurance/private/fault_coverage` --- SWIFI scenario catalogue (FI-13, FI-14)
