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

Module Layout
-------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Module
     - Description
   * - ``prn``
     - Code generation and lookup for Gold-2046, Weil-10230, and Weil-1500.
   * - ``modulator``
     - BPSK mapping for I (1.023 Mcps) and Q (5.115 Mcps) channels.
   * - ``matched_code``
     - AFS-Q composite code generation via XORing tiered components.
   * - ``iq_mux``
     - 50/50 power multiplexing and sample-and-hold upsampling (1x I → 5x Q).
   * - ``bch``
     - BCH(51,8) encoding and ML correlation-based decoding for SB1.
   * - ``frame``
     - Navigation frame construction, including sync pattern insertion.

Python Integration
------------------

The Python layer (``lunalink.afs``) provides a NumPy-friendly interface to the
C++ core using ``pybind11``. It is intended for:
*   Rapid prototyping of signal chains.
*   Automated test vector generation.
*   Visualization and link-budget analysis.

For high-rate SDR or hardware integration, the C++ library can be used directly
via the headers in ``cpp/include/lunalink/``.
