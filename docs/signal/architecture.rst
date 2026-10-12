Architecture
============

The LunaLink Signal-In-Space (SIS) library is a mission-critical C++ core with
optional Python bindings.

Core Design Decisions
---------------------

1.  **No Exceptions & No RTTI**: the C++ core compiles with ``-fno-exceptions``
    and ``-fno-rtti``, for deterministic behaviour and compatibility with the
    embedded flight-software constraints of the ECSS and NASA standards.
2.  **Stateless Signal Chain**: most core functions (modulators, encoders,
    multiplexers) are stateless and take ``std::span``, which buys memory
    safety without dynamic allocation.
3.  **Static Table Storage**: Spreading codes (Gold, Weil) are stored as packed
    bitstreams in static storage (``prn_table_*.cpp``) to minimize startup time
    and memory footprint.
4.  **Bitwise Performance**: critical paths such as the BCH(51,8) LFSR and the
    matched-code combiners use bitwise logic and C++20 features like
    ``std::popcount``.

Mission Assurance Integration
-----------------------------

**Class B Flight Software** standards bring in the safety-critical patterns
defined in :doc:`../assurance/mission_assurance`:

*   **TMR Protection**: critical identifiers such as ``PrnId`` and ``Fid``
    carry Triple Modular Redundancy with active repair.
*   **Control-Flow Integrity**: Safety-critical loops (BCH, LDPC, PRN) implement
    sentinel counters to detect radiation-induced logic jumps.
*   **Memory Pinning**: Large look-up tables are mapped to the
    ``.lunalink_lut`` section for physical memory placement control.

Module map
----------

.. list-table::
   :header-rows: 1
   :widths: 20 40 40

   * - Module
     - Component
     - Guide
   * - ``prn``
     - Spreading code loader
     - :doc:`/verification/spreading_codes`
   * - ``matched_code``
     - Tiered code combiner
     - :doc:`/verification/spreading_codes`
   * - ``bch``
     - BCH(51,8) codec
     - :doc:`/verification/error_correction`
   * - ``ldpc``
     - LDPC encoder and decoder
     - :doc:`/verification/error_correction`
   * - ``frame``
     - Message framing
     - :doc:`/verification/framing`
   * - ``modulator``
     - BPSK modulation
     - :doc:`/verification/baseband`
   * - ``iq_mux``
     - IQ multiplexing
     - :doc:`/verification/baseband`

Python Integration
------------------

The Python layer (``lunalink.afs``) wraps the C++ core in a NumPy-friendly
interface via ``nanobind``.  It suits:
*   Rapid prototyping of signal chains.
*   Automated test vector generation.
*   Visualization and link-budget analysis.

High-rate SDR and hardware integration should reach the C++ library directly,
through the headers in ``cpp/include/lunalink/``.
