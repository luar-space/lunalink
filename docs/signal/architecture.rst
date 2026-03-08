Architecture
============

The LunaLink Signal-In-Space (SIS) library is designed as a high-performance,
mission-critical C++ core with optional Python bindings.

Core Design Decisions
---------------------

1.  **No Exceptions & No RTTI**: The C++ core is compiled with ``-fno-exceptions``
    and ``-fno-rtti`` to ensure deterministic behavior and compatibility with
    embedded flight software constraints (ECSS/NASA standards).
2.  **Stateless Signal Chain**: Most core functions (modulators, encoders, multiplexers)
    are designed as stateless functions using ``std::span`` for memory safety
    without the overhead of dynamic allocation.
3.  **Static Table Storage**: Spreading codes (Gold, Weil) are stored as packed
    bitstreams in static storage (``prn_table_*.cpp``) to minimize startup time
    and memory footprint.
4.  **Bitwise Performance**: Critical paths like the BCH(51,8) LFSR and Matched-Code
    combiners are implemented using bitwise logic and modern C++20 features
    (e.g., ``std::popcount``) for aerospace-grade performance.

Mission Assurance Integration
-----------------------------

To satisfy **Class A Flight Software** standards, the architecture incorporates 
safety-critical patterns defined in :doc:`../assurance/mission_assurance`:

*   **TMR Protection**: Critical identifiers like ``PrnId`` and ``Fid`` utilize 
    Triple Modular Redundancy with active repair.
*   **Control-Flow Integrity**: Safety-critical loops (BCH, LDPC, PRN) implement 
    sentinel counters to detect radiation-induced logic jumps.
*   **Memory Pinning**: Large look-up tables are mapped to the 
    ``.lunalink_lut`` section for physical memory placement control.

Module & Gateway Mapping
------------------------

.. list-table::
   :header-rows: 1
   :widths: 20 60 20

   * - Module
     - Deliverable
     - Gateway
   * - ``prn``
     - Spreading Code Loader (GD1.1)
     - G1
   * - ``matched_code``
     - Tiered Code Combiner (GD1.2)
     - G1
   * - ``bch``
     - BCH(51,8) Codec (GD2.1)
     - G2
   * - ``ldpc``
     - LDPC Encoder/Decoder (GD2.2)
     - G2
   * - ``frame``
     - Message Framing (GD3.1)
     - G3
   * - ``modulator``
     - BPSK Modulation (GD4.1)
     - G4
   * - ``iq_mux``
     - IQ Multiplexing (GD4.2)
     - G4

Python Integration
------------------

The Python layer (``lunalink.afs``) provides a NumPy-friendly interface to the
C++ core using ``pybind11``. It is intended for:
*   Rapid prototyping of signal chains.
*   Automated test vector generation.
*   Visualization and link-budget analysis.

For high-rate SDR or hardware integration, the C++ library can be used directly
via the headers in ``cpp/include/lunalink/``.
