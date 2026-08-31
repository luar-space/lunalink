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

* The public C++ API exported from ``liblunalink``.
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

The Hardware Abstraction Layer decouples the signal chain from platform
specifics.  Both interfaces are pure-virtual C++ abstract classes; concrete
implementations are injected at construction time.

**ITelemetryHal** (``cpp/include/lunalink/signal/hal.hpp``)

.. list-table::
   :header-rows: 1
   :widths: 30 30 40

   * - Method
     - Signature
     - Description
   * - ``emit_fault``
     - ``virtual void emit_fault(FaultPacket const&) noexcept = 0``
     - Called by the signal chain whenever ``kFaultDetected`` is raised.
       Implementations must not block or allocate.
   * - ``emit_heartbeat``
     - ``virtual void emit_heartbeat(uint32_t frame_id) noexcept = 0``
     - Called once per frame to confirm liveness.  Frame ID wraps at
       ``UINT32_MAX``.

**IWatchdogHal** *(future — Gateway 5 deliverable)*

.. list-table::
   :header-rows: 1
   :widths: 30 30 40

   * - Method
     - Signature
     - Description
   * - ``hw_kick``
     - ``virtual void hw_kick() noexcept = 0``
     - Kicks the hardware watchdog peripheral.  Called inside
       ``WindowedWatchdog::kick()`` after the software window check passes.
   * - ``hw_reset``
     - ``virtual void hw_reset() noexcept = 0``
     - Forces a system reset.  Called when the software watchdog expires
       and no recovery is possible.

**FaultPacket encoding** (``cpp/include/lunalink/signal/fault_packet.hpp``)

``FaultPacket`` is a 16-byte, POD-compatible structure packed as follows:

.. code-block:: text

   Byte 0–1  : Magic (0xFA, 0xCE)
   Byte 2    : FIZ level (1–4, per the fault-isolation zones in fault_coverage.rst)
   Byte 3    : Severity (0x01 = informational … 0xFF = safety-critical halt)
   Byte 4–7  : Error code (AfsErrorCode, little-endian uint32)
   Byte 8–11 : Frame ID at time of fault (little-endian uint32)
   Byte 12–15: Reserved (0x00)

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

    Output length is always 2 × input length.  Both I and Q inputs must have
    identical length; a ``kLengthMismatch`` error is returned otherwise.

**FaultPacket byte layout**
    Described in §HAL Interface above.  The structure is always 16 bytes,
    zero-padded.  The magic bytes allow receivers to validate packet framing.

Error Code Table
-----------------

All error codes are members of ``enum class AfsErrorCode : uint32_t``
(``cpp/include/lunalink/signal/error.hpp``).

.. list-table::
   :header-rows: 1
   :widths: 28 12 40 20

   * - Enumerator
     - Hex Value
     - Description
     - FIZ Level
   * - ``kOk``
     - ``0x0000``
     - Operation succeeded; no fault
     - N/A
   * - ``kFaultDetected``
     - ``0x0001``
     - TMR vote mismatch or CFI guard mismatch detected
     - FIZ-1 (signal chain halt)
   * - ``kInvalidPrn``
     - ``0x0002``
     - PRN ID outside valid range 1–210
     - FIZ-2 (function return; caller must handle)
   * - ``kBoundsViolation``
     - ``0x0003``
     - ``CheckedRange`` index out of bounds
     - FIZ-1 (safety-critical; chain halted)
   * - ``kLengthMismatch``
     - ``0x0004``
     - I and Q chip span lengths differ
     - FIZ-2 (function return)
   * - ``kWatchdogExpired``
     - ``0x0005``
     - Software windowed watchdog expired without a valid kick
     - FIZ-1 (signal chain halt)
   * - ``kInvalidStateTransition``
     - ``0x0006``
     - FSM received an event invalid for the current state
     - FIZ-2 (FSM remains in previous state; caller notified)
   * - ``kPostFailed``
     - ``0x0007``
     - Power-On Self-Test returned failure on one or more checks
     - FIZ-1 (system must not enter operational mode)
   * - ``kCrcMismatch``
     - ``0x0008``
     - CRC-24Q check failed on received message
     - FIZ-3 (message discarded; frame processing continues)
   * - ``kFecUncorrectable``
     - ``0x0009``
     - BCH/LDPC decoder encountered uncorrectable error pattern
     - FIZ-3 (subframe discarded; frame processing continues)
