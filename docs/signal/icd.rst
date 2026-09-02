Interface Control Document
===========================

**Document ID**: LUNALINK-ICD-001
**Specification Reference**: NPR 7150.2D SWE-090
**Revision**: 1.0
**Date**: 2026-03-12
**Status**: Released — Competition Submission Baseline

Purpose
-------

This Interface Control Document (ICD) defines all externally visible interfaces
of the LunaLink LSIS-AFS library.  It covers:

* The public C++ API exported from ``lunalink_core``.
* The Python bindings exposed through the ``_afs`` nanobind extension module.
* The Hardware Abstraction Layer (HAL) interfaces for telemetry and watchdog.
* Wire-level data formats (chip values, IQ interleave, ``FaultPacket``).
* The complete error code enumeration with FIZ (Fault Isolation Zone)
  classification.

This document satisfies NPR 7150.2D SWE-090 ("the project shall produce an
Interface Control Document for each interface between software elements and
between the software and hardware").

C++ API Interface
------------------

All public functions and types are declared in headers under
``cpp/include/lunalink/signal/``.  Unless otherwise noted, all functions are
**not thread-safe** (no internal locking; callers must serialise access to any
shared object) but **re-entrant**: computation depends on no global mutable
state, so concurrent calls operating on distinct objects and workspaces are
safe.  The only file-scope mutable is the ``volatile`` watchdog liveness
counter ``g_wip_count`` (``core/safety.hpp``), whose value never feeds any
computation.

.. list-table::
   :header-rows: 1
   :widths: 22 22 16 16 12 12

   * - Name
     - Signature
     - Inputs
     - Outputs / Return
     - Error Codes
     - Thread Safety
   * - ``modulate_bpsk_i``
     - ``ModulationStatus modulate_bpsk_i(prn_chips, data_symbol, out) noexcept``
     - ``prn_chips``: span of {0, 1} chips; ``data_symbol``: one symbol in
       {-1, +1}; ``out``: output span
     - ``out`` filled with BPSK chips {-1, +1}; ``ModulationStatus``
     - ``kInvalidSymbol``, ``kInvalidChipValue``, ``kOutputTooSmall``,
       ``kFaultDetected``
     - Not thread-safe; re-entrant
   * - ``modulate_bpsk_q``
     - ``ModulationStatus modulate_bpsk_q(chips, out) noexcept``
     - ``chips``: span of {0, 1} matched-code chips (pilot channel — data
       symbol fixed at +1); ``out``: output span
     - ``out`` filled with BPSK chips {-1, +1}; ``ModulationStatus``
     - ``kInvalidChipValue``, ``kOutputTooSmall``, ``kFaultDetected``
     - Not thread-safe; re-entrant
   * - ``multiplex_iq``
     - ``IqMuxStatus multiplex_iq(i_samples, q_samples, out) noexcept``
     - ``i_samples``, ``q_samples``: equal-length spans of int8 {-1, +1}
     - ``out`` interleaved as [I0, Q0, I1, Q1, …]; ``IqMuxStatus``
     - ``kInputTooSmall``, ``kOutputTooSmall``, ``kInvalidISample``,
       ``kInvalidQSample``, ``kNullInput``
     - Not thread-safe; re-entrant
   * - ``run_post``
     - ``PostResult run_post() noexcept``
     - None
     - ``PostResult{status, failed_step}`` — ``kPass`` with
       ``failed_step == 0``, or ``kFaultDetected`` with the 1–5 step index
     - ``kFaultDetected``
     - Not thread-safe; re-entrant
   * - ``commit_frame``
     - ``FrameStatus commit_frame(std::span<const uint8_t> frame_buf) noexcept``
     - ``frame_buf``: assembled frame buffer with trailing CRC-24Q
     - ``FrameStatus`` — ``kOk`` when the trailing CRC-24Q verifies
     - ``kFaultDetected``
     - Not thread-safe; re-entrant
   * - ``SignalChainFsm``
     - ``class SignalChainFsm`` (``explicit SignalChainFsm(ITelemetryHal&)``)
     - Transitions via ``[[nodiscard]] bool transition(...)``
     - States: ``kUninitialized``, ``kPostRunning``, ``kOperational``,
       ``kFrameFault``, ``kSafeMode``; illegal edges return ``false`` and
       emit FIZ-3 telemetry
     - *(bool return; FIZ-3 telemetry)*
     - Not thread-safe; single-owner
   * - ``WindowedWatchdog``
     - ``class WindowedWatchdog`` (``constexpr WindowedWatchdog(t_min_ticks, t_max_ticks)``)
     - ``tick()`` each processed unit; ``evaluate(hal, timestamp_ms)`` once
       per window; ``reset()``
     - ``evaluate`` returns ``false`` when the tick count fell below
       ``t_min`` (FIZ-2); ``upper_bound_exceeded()`` flags overrun
     - *(bool return; FIZ-2 telemetry)*
     - Not thread-safe; re-entrant
   * - ``TmrValue<T>``
     - ``template<typename T> struct TmrValue``
     - Three independent copies of ``T``; ``set(T val)`` writes all three
     - ``T vote(bool& fault)`` — majority vote with active repair: a single
       corrupted copy is scrubbed back during ``vote()``
     - ``fault`` set on 3-way disagreement
     - Not thread-safe; re-entrant
   * - ``CheckedRange<T, Min, Max>``
     - ``template<typename T, T Min, T Max> class CheckedRange``
     - Range-bounded value wrapper; construction and assignment saturate
       the stored value into [``Min``, ``Max``]
     - Always-in-range value via ``get()``
     - *(saturating; no error path)*
     - Not thread-safe; re-entrant
   * - ``SyncDetector``
     - ``class SyncDetector`` (``explicit SyncDetector(ITelemetryHal&)``)
     - ``set_threshold(float)``; ``detect(rx_symbols, result)`` with
       ``rx_symbols``: span of soft symbols
     - ``SyncResult`` struct (``frame_offset``, ``peak_metric``,
       ``second_peak``, ``detected``)
     - ``SyncDetectorStatus::kInvalidInput``, ``kNotDetected``,
       ``kFaultDetected``
     - Not thread-safe (holds TMR state); single-owner
   * - ``CfiGuard``
     - ``class CfiGuard`` (``constexpr CfiGuard(magic, n)``)
     - Counts guarded control-flow steps against an expected count ``n``
     - ``verify()`` returns ``false`` when the executed count differs from
       ``n`` (control-flow-integrity breach)
     - *(bool return)*
     - Not thread-safe; re-entrant

Python API Interface
---------------------

The ``_afs`` nanobind extension module is imported as ``lunalink.afs._afs``.
The public Python wrapper layer in ``src/lunalink/afs/signal.py`` re-exports
all names listed below under the ``lunalink.afs`` namespace.

.. list-table::
   :header-rows: 1
   :widths: 24 30 22 24

   * - Python Name
     - Signature (Python)
     - Description
     - Raises
   * - ``modulate_i``
     - ``modulate_i(prn: ndarray[uint8], data: int) -> ndarray[int8]``
     - Wraps C++ ``modulate_bpsk_i``; spreads one data symbol over the PRN
       chips, returning {-1, +1} int8 chips
     - ``ValueError`` on invalid chips or symbol
   * - ``modulate_q``
     - ``modulate_q(chips: ndarray[uint8]) -> ndarray[int8]``
     - Wraps C++ ``modulate_bpsk_q`` (pilot channel; data symbol fixed
       at +1)
     - ``ValueError`` on invalid chips
   * - ``multiplex_iq``
     - ``multiplex_iq(i: ndarray[int8], q: ndarray[int8]) -> ndarray[int8]``
     - Interleaves I and Q chips as [I0, Q0, I1, Q1, …]; output length =
       2 × len(i)
     - ``ValueError`` on length mismatch or invalid sample

The platform-layer facilities (``run_post``, ``SignalChainFsm``,
``WindowedWatchdog``) are deliberately **not** exposed to Python — they are
flight-software scaffolding exercised by the C++ unit and SWIFI suites.

Type stubs are maintained in ``src/lunalink/afs/_afs.pyi``.

Core Signal-Chain API
~~~~~~~~~~~~~~~~~~~~~~

The end-to-end encode/decode chain is exposed under ``lunalink.afs``
(re-exported from ``src/lunalink/afs/__init__.py``).  Full signatures,
parameter tables, and runnable examples are in the API reference
(:doc:`../api`); this table indexes the externally visible entry points.

.. list-table::
   :header-rows: 1
   :widths: 30 50 20

   * - Function
     - Purpose
     - Documented in
   * - ``prn_code``
     - Gold-2046 AFS-I primary spreading code for PRN 1–210
     - :doc:`../api`
   * - ``weil10230_code`` / ``weil1500_code``
     - Weil AFS-Q primary (10230) and tertiary (1500) codes
     - :doc:`../api`
   * - ``matched_code_epoch`` / ``default_matched_assignment``
     - AFS-Q matched-code combination (primary ⊕ secondary ⊕ tertiary)
     - :doc:`../api`
   * - ``bch_encode`` / ``bch_decode`` / ``bch_decode_soft``
     - BCH(51,8) SB1 header encode and hard/soft ML decode
     - :doc:`../api`, :doc:`bch_decoder_design`
   * - ``crc24_calculate`` / ``crc24_verify``
     - CRC-24Q generation and verification
     - :doc:`../api`
   * - ``ldpc_encode`` / ``ldpc_decode``
     - Rate-1/2 LDPC: SF2 (2400,1200), SF3/SF4 (1740,870)
     - :doc:`../api`, :doc:`ldpc_algo_card`
   * - ``block_interleave`` / ``block_deinterleave``
     - 60×98 block (de)interleaver, hard and soft variants
     - :doc:`../api`
   * - ``frame_build`` / ``subframe_build_sb2/3/4``
     - Assemble the 6000-symbol, 12 s navigation frame
     - :doc:`../api`
   * - ``generate_iq_frame`` / ``generate_iq_epoch``
     - Baseband I/Q sample generation
     - :doc:`../api`
   * - ``transmit_frame`` / ``receive_frame``
     - One-call TX pipeline and full RX pipeline (sync → BCH →
       deinterleave → LDPC → CRC → parse)
     - :doc:`../api`
   * - ``compute_tot`` / ``compute_sise``
     - Time-of-transmission and SISE utilities
     - :doc:`../api`

HAL Interface
--------------

The Hardware Abstraction Layer is one pure-virtual interface,
``ITelemetryHal`` (``cpp/include/lunalink/core/telemetry.hpp``).  The signal
chain never depends on how a platform delivers telemetry: a reference to the
interface is injected at construction (``SyncDetector``, ``SignalChainFsm``)
or passed per call (``frame_decode``, ``WindowedWatchdog::evaluate``).  Two
implementations ship with it — ``NullTelemetryHal`` discards packets and is
the default in non-flight stubs; ``CaptureTelemetryHal<N>`` stores up to
``N`` packets for test assertions.

.. list-table::
   :header-rows: 1
   :widths: 20 40 40

   * - Method
     - Signature
     - Description
   * - ``emit``
     - ``virtual void emit(const FaultPacket&) noexcept = 0``
     - Called when a fault escalates beyond the block level — from
       ``frame_decode`` and the sync detector, on an illegal
       ``SignalChainFsm`` transition, and when the windowed watchdog's
       *lower* bound is violated.  Implementations must not block, allocate
       or throw.

**Platform components that report through it**

.. list-table::
   :header-rows: 1
   :widths: 26 74

   * - Component
     - Interface
   * - ``WindowedWatchdog``
       (``platform/watchdog.hpp``)
     - ``WindowedWatchdog(t_min_ticks, t_max_ticks)``; ``tick()`` once per
       unit of proven progress; ``evaluate(hal, timestamp_ms)`` at the end
       of the window emits a ``FaultPacket`` with fault code ``0xCC`` if
       fewer than ``t_min`` ticks were seen (a jump *over* the loop), and
       ``upper_bound_exceeded()`` reports the classic hang case;
       ``reset()`` opens the next window.
   * - ``SignalChainFsm``
       (``platform/signal_chain_fsm.hpp``)
     - ``SignalChainFsm(hal)``; ``transition(next)`` is checked against the
       legal-transition matrix and emits on refusal; ``on_frame_fault()``
       moves to ``kFrameFault`` (Zone 2), ``on_frame_ok()`` back to
       ``kOperational``; ``state()`` reads the SEU-resistant state
       (``kUninitialized`` 0x11, ``kPostRunning`` 0x22, ``kOperational``
       0x44, ``kFrameFault`` 0x55, ``kSafeMode`` 0x99).
   * - POST (``platform/post.hpp``)
     - Returns ``PostStatus::kPass`` (0x5A) or ``kFaultDetected`` (0x99);
       step 1 verifies the SCI header of the running binary.

No hardware watchdog kick or reset line is part of this release's
interface: the software watchdog reports through ``emit``, and what a
platform does with a Zone-3 packet is the integrator's decision.

**FaultPacket encoding** (``cpp/include/lunalink/core/telemetry.hpp``)

``FaultPacket`` is an 8-byte POD structure (``static_assert``-ed), emitted
before a Zone-3 halt [SW-SAF-03]:

.. code-block:: text

   Byte 0–3 : timestamp_ms      monotonic millisecond counter, HAL-provided
                                (native uint32_t)
   Byte 4   : fault_code        module-specific discriminator — the FSM emits
                                the destination state (kSafeMode = 0x99), the
                                watchdog emits 0xCC
   Byte 5   : fiz_level         Fault Isolation Zone 1, 2 or 3
   Byte 6   : originating_step  POST step (1–5), FSM state, or 0
   Byte 7   : reserved          0x00

Data Formats
-------------

**Chip values**
    All chip sequences use the bipolar convention of LSIS Table 8: logical 0 →
    chip value +1, logical 1 → chip value -1 (implemented as the branchless
    mapping in ``modulator.hpp``).  In C++ the type is ``int8_t``; in Python
    it is a plain ``int`` in {-1, +1}.  Callers must not pass any other value.

**IQ interleave format**
    ``multiplex_iq`` produces an interleaved sample stream ordered as::

        [I₀, Q₀, I₁, Q₁, I₂, Q₂, …, Iₙ₋₁, Qₙ₋₁]

    It works one code epoch at a time: 2046 AFS-I chips and 10230 AFS-Q chips
    in, 20460 interleaved ``int16`` samples out — each I chip is held for the
    five Q chips it spans, since the chip rates are 1.023 and 5.115 Mchip/s.
    A shorter input returns ``IqMuxStatus::kInputTooSmall``, an output that
    cannot hold 20460 samples returns ``kOutputTooSmall``, and a chip outside
    {-1, +1} returns ``kInvalidISample`` / ``kInvalidQSample``; nothing is
    written on any of them.

**FaultPacket byte layout**
    Described in §HAL Interface above: 8 bytes, native struct layout, the
    ``reserved`` byte always zero.  Framing and any transport encoding are
    the telemetry HAL implementation's responsibility.

Error Code Table
-----------------

Every fallible function returns a subsystem status enumeration rather than
throwing; the build is ``-fno-exceptions``.  Each enumeration is
``enum class … : uint8_t`` with **SEU-resistant values** — non-contiguous
codes of Hamming distance ≥ 4 from one another (``0x5A``, ``0xA5``,
``0x33``, ``0xCC``, …), so a single bit flip in a stored status yields an
*illegal* value rather than a different legal one.  Success is ``kOk``
(``0x5A``) everywhere, and ``kFaultDetected`` is the CFI/TMR/integrity
failure that also raises a ``FaultPacket`` through the HAL.

.. list-table::
   :header-rows: 1
   :widths: 24 22 54

   * - Enumeration (header)
     - Enumerators besides ``kOk`` / ``kFaultDetected``
     - Containment on a non-``kOk`` return
   * - ``PrnStatus``
       (``codes/prn_codes.hpp``)
     - ``kInvalidPrn`` (PRN outside 1–210), ``kNullOutput``,
       ``kInvalidChipIndex``
     - Argument error: returned without writing output; caller handles
   * - ``LegendreStatus`` (``codes/legendre.hpp``)
     - ``kOutputTooSmall``, ``kInvalidPrime`` (not in {1499, 10223})
     - Argument error
   * - ``MatchedCodeStatus`` (``codes/matched_code.hpp``)
     - ``kInvalidPrn``, ``kInvalidEpoch``, ``kOutputTooSmall``,
       ``kInvalidAssignment``
     - Argument error
   * - ``BchStatus`` (``encoding/bch.hpp``)
     - ``kOutputTooSmall``, ``kInvalidFid``, ``kInvalidToi``,
       ``kNullOutput``, ``kInvalidInput``, ``kAmbiguousMatch`` (two
       codewords at equal Hamming distance)
     - ``kAmbiguousMatch`` is a decode failure: the frame is discarded at
       the frame boundary (Zone 2) and the chain resumes with the next one;
       the rest are argument errors
   * - ``LdpcStatus`` (``encoding/ldpc.hpp``)
     - ``kInvalidInput``, ``kOutputTooSmall``, ``kNotConverged``
       (iteration cap reached without a satisfied syndrome)
     - ``kNotConverged`` is a decode failure (Zone 2, frame discarded);
       the rest are argument errors
   * - ``CrcStatus`` (``encoding/crc.hpp``)
     - ``kNullInput``, ``kOutputTooSmall``
     - Argument errors; a CRC *mismatch* is reported as ``false`` from
       ``crc24_verify`` and as ``FrameDecodeStatus::kCrcFailed``
   * - ``InterleaverStatus`` (``encoding/interleaver.hpp``)
     - ``kInputTooSmall``, ``kOutputTooSmall``
     - Argument error
   * - ``FrameStatus`` (``messages/frame.hpp``)
     - ``kOutputTooSmall``, ``kInvalidFid``, ``kInvalidToi``,
       ``kBchFailed``, ``kLdpcFailed``, ``kCrcFailed``
     - Argument errors on the input checks; the decode failures discard
       the frame (Zone 2)
   * - ``FrameDecodeStatus`` (``messages/frame_decode.hpp``)
     - ``kSyncNotFound``, ``kBchFailed``, ``kLdpcFailed``, ``kCrcFailed``,
       ``kInvalidInput``
     - Zone 2 — the frame is discarded and the next one is processed; the
       Python layer maps these to ``SyncNotFoundError`` and friends
   * - ``MessageParseStatus`` (``messages/message_parser.hpp``)
     - ``kInvalidInput``
     - Argument error
   * - ``TotStatus`` (``messages/time_of_transmission.hpp``)
     - ``kInvalidToi``, ``kInvalidItow``, ``kInvalidWn``, ``kInvalidDelta``
       (NaN/Inf)
     - Argument error
   * - ``ModulationStatus`` (``signal/modulator.hpp``)
     - ``kInvalidSymbol``, ``kInvalidChipValue``, ``kOutputTooSmall``
     - Argument error
   * - ``IqMuxStatus`` (``signal/iq_mux.hpp``)
     - ``kInputTooSmall``, ``kOutputTooSmall``, ``kInvalidISample``,
       ``kInvalidQSample``, ``kNullInput``
     - Argument error, returned before anything is written; ignoring the
       status would let malformed I/Q reach the RF stage, hence the
       ``[[nodiscard]]`` message on the function
   * - ``SignalGenStatus`` (``signal/signal_gen.hpp``)
     - ``kInvalidPrn``, ``kInvalidInput``, ``kModulationFailed``,
       ``kMuxFailed``
     - Argument error, or a propagated failure from the stage named
   * - ``SyncDetectorStatus`` (``signal/sync_detector.hpp``)
     - ``kInvalidInput``, ``kNotDetected`` (peak below threshold)
     - ``kNotDetected`` discards the frame (Zone 2); ``kInvalidInput`` is an
       argument error
   * - ``SiseStatus`` (``core/sise.hpp``)
     - ``kInvalidInput`` (NaN/Inf or null), ``kEmptyData``
     - Argument error
   * - ``PostStatus`` (``platform/post.hpp``)
     - *(none — ``kPass`` 0x5A or ``kFaultDetected``)*
     - Zone 3 — the system must not enter ``kOperational``

``kFaultDetected`` follows the Fault Isolation Zones of
:doc:`../assurance/mission_assurance`: at the block where it is detected
(Zone 1) the function scrubs its buffers and returns without writing
output; the caller applies frame-level policy (Zone 2), discarding the
frame and resuming with the next; two consecutive frame faults, a failed
integrity check or a legal-transition-matrix miss escalate to Zone 3 —
``SignalChainFsm`` enters ``kSafeMode``, transmission halts and a
``FaultPacket`` is emitted.  The exact values of every enumerator are in the header
named, and in :doc:`../api_cpp`; the tests reference them by name, so a
renumbering is caught at compile time rather than by this table.
