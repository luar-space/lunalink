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
* The complete error code enumeration with FIZ classification.

This document satisfies NPR 7150.2D SWE-090 ("the project shall produce an
Interface Control Document for each interface between software elements and
between the software and hardware").

C++ API Interface
------------------

All public functions and types are declared in headers under
``cpp/include/lunalink/signal/``.  Unless otherwise noted, all functions are
**thread-hostile** (no internal locking; callers must serialise access) and
**reentrant-safe** (no global mutable state).

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
     - ``Result<ChipSpan> modulate_bpsk_i(DataBitSpan bits, PrnId id)``
     - ``bits``: read-only span of data bits; ``id``: PRN identifier 1–210
     - ``Result<ChipSpan>`` containing output chip sequence {-1, +1}
     - ``kInvalidPrn``, ``kBoundsViolation``
     - Thread-hostile
   * - ``modulate_bpsk_q``
     - ``Result<ChipSpan> modulate_bpsk_q(DataBitSpan bits, PrnId id)``
     - ``bits``: read-only span of data bits; ``id``: PRN identifier 1–210
     - ``Result<ChipSpan>`` containing output chip sequence {-1, +1}
     - ``kInvalidPrn``, ``kBoundsViolation``
     - Thread-hostile
   * - ``multiplex_iq``
     - ``Result<IqSpan> multiplex_iq(ChipSpan i_chips, ChipSpan q_chips)``
     - ``i_chips``: I-channel chips; ``q_chips``: Q-channel chips (must be same length)
     - ``Result<IqSpan>`` interleaved as [I0, Q0, I1, Q1, …]
     - ``kLengthMismatch``, ``kBoundsViolation``
     - Thread-hostile
   * - ``run_post``
     - ``bool run_post()``
     - None
     - ``true`` if all Power-On Self-Test checks pass; ``false`` on any failure
     - *(none; returns false)*
     - Thread-hostile
   * - ``commit_frame``
     - ``Result<void> commit_frame(FrameBuffer& buf, const FrameParams& p)``
     - ``buf``: mutable frame buffer to finalise; ``p``: frame parameters (PRN, ToT, subframe IDs)
     - ``Result<void>`` (ok or error code)
     - ``kWatchdogExpired``, ``kFaultDetected``, ``kBoundsViolation``
     - Thread-hostile
   * - ``SignalChainFsm``
     - ``class SignalChainFsm``
     - Constructed with ``HalBundle&``; transitions via ``advance(Event)``
     - State enum ``{Idle, Armed, Transmitting, Faulted}``
     - ``kInvalidStateTransition``, ``kFaultDetected``
     - Thread-hostile; single-owner
   * - ``WindowedWatchdog``
     - ``class WindowedWatchdog``
     - Constructed with ``min_ticks``, ``max_ticks``; ``kick()`` must be called each frame
     - ``bool is_expired() const``; ``bool kick()``
     - ``kWatchdogExpired`` (via ``commit_frame`` return path)
     - Thread-hostile
   * - ``TmrValue<T>``
     - ``template<typename T> class TmrValue``
     - Three independent copies of ``T``; ``set(T val)`` writes all three
     - ``Result<T> vote() const`` — majority vote; ``kFaultDetected`` on disagreement
     - ``kFaultDetected``
     - Thread-hostile
   * - ``CheckedRange<T,N>``
     - ``template<typename T, std::size_t N> class CheckedRange``
     - ``operator[](std::size_t idx)`` — bounds-checked array access
     - ``Result<T&>`` on read; ``kBoundsViolation`` if ``idx >= N``
     - ``kBoundsViolation``
     - Thread-hostile
   * - ``SyncDetector``
     - ``class SyncDetector``
     - Constructed with ``ITelemetryHal&``; ``set_threshold(float)``;
       ``detect(rx_symbols, result)``
     - ``SyncResult`` struct (frame_offset, peak_metric, second_peak,
       detected)
     - ``SyncDetectorStatus::kInvalidInput``, ``kNotDetected``,
       ``kFaultDetected``
     - Thread-hostile
   * - ``CfiGuard<T>``
     - ``template<typename T> class CfiGuard``
     - Wraps enum ``T``; stores complement-pair; ``get()`` validates both copies
     - ``Result<T> get() const``; ``kFaultDetected`` on mismatch
     - ``kFaultDetected``
     - Thread-hostile

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
   * - ``modulate_bpsk_i``
     - ``modulate_bpsk_i(bits: list[int], prn_id: int) -> list[int]``
     - Wraps C++ ``modulate_bpsk_i``; chips returned as Python list of {-1, +1}
     - ``ValueError`` on invalid PRN; ``RuntimeError`` on internal fault
   * - ``modulate_bpsk_q``
     - ``modulate_bpsk_q(bits: list[int], prn_id: int) -> list[int]``
     - Wraps C++ ``modulate_bpsk_q``
     - ``ValueError``, ``RuntimeError``
   * - ``multiplex_iq``
     - ``multiplex_iq(i: list[int], q: list[int]) -> list[int]``
     - Interleaves I and Q chip lists; output length = 2 × len(i)
     - ``ValueError`` on length mismatch
   * - ``run_post``
     - ``run_post() -> bool``
     - Runs POST; returns ``True`` on pass
     - *(none)*
   * - ``SignalChainFsm``
     - ``class SignalChainFsm``
     - Python handle to the C++ FSM; ``advance(event: int) -> None``
     - ``RuntimeError`` on invalid transition or fault
   * - ``AfsErrorCode``
     - ``class AfsErrorCode(IntEnum)``
     - Python mirror of the C++ ``AfsErrorCode`` enum
     - *(none)*

Type stubs are maintained in ``src/lunalink/afs/_afs.pyi``.

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
    All chip sequences use the bipolar convention: logical 1 → chip value +1,
    logical 0 → chip value -1.  In C++ the type is ``int8_t``; in Python
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
