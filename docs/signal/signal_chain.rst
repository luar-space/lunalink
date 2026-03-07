Signal Chain (C1–C4)
====================

The LunaLink signal generation pipeline follows the LSIS V1.0 Augmented Forward Signal (AFS) specification. It is composed of modular, stateless components that transform navigation data into baseband I/Q samples.

Block Diagram
-------------

.. code-block:: text

    ┌─────────────────┐     ┌──────────────────────┐
    │ PRN Hex Tables   │     │  Secondary Codes      │
    │ (Gold-2046)      │     │  S0–S3 (4-bit)        │
    └────────┬────────┘     └──────────┬───────────┘
             │                          │
             ▼                          ▼
    ┌─────────────────┐     ┌──────────────────────┐
    │  C1 Code Loader  │     │  Tertiary Code         │
    │  (Gold-2046)     │     │  (Weil-1500)           │
    └────────┬────────┘     └──────────┬───────────┘
             │                          │
             │                          │
             ▼                          │
    ┌─────────────────┐                 │
    │  C2 BPSK(1)     │                 │
    │  Modulator      │                 │
    │                 │                 │
    │  1.023 Mcps     │                 │
    │  int8 {-1, +1}  │                 │
    └────────┬────────┘                 │
             │                          │
             │                          │
             │                          ▼
             │                ┌─────────────────┐
             │                │  C3 Matched-Code│
             │                │  Combiner       │
             │                │                 │
             │                │  primary ⊕      │
             │                │  secondary ⊕    │
             │                │  tertiary       │
             │                │                 │
             │                │  5.115 Mcps     │
             │                │  uint8 {0, 1}   │
             │                └────────┬────────┘
             │                         │
             │                         ▼
             │                ┌─────────────────┐
             │                │  C2 BPSK(5)     │
             │                │  Modulator      │
             │                │                 │
             │                │  5.115 Mcps     │
             │                │  int8 {-1, +1}  │
             │                └────────┬────────┘
             │                         │
             │                         │
             ▼                         ▼
    ┌──────────────────────────────────────────────┐
    │                                              │
    │              C4 IQ Multiplexer               │
    │                                              │
    │  AFS-I (I channel)      AFS-Q (Q channel)    │
    │  Upsampled 5x           Pass-through         │
    │                                              │
    └──────────────────────┬───────────────────────┘
                           │
                           ▼
                  Baseband I/Q Output
                  (5.115 MSPS, int16)

Data Flow
---------

1.  **Code Loading (C1)**:
    Static tables (``prn_table_*.cpp``) are queried to retrieve the full chip sequences for the Primary (Gold-2046, Weil-10230) and Tertiary (Weil-1500) codes. This avoids runtime LFSR generation.

2.  **Matched-Code Combination (C3)**:
    For the AFS-Q pilot channel, three components are XORed together to form the final ranging code:
    
    *   **Primary**: Weil-10230 code (10230 chips, 2 ms epoch).
    *   **Secondary**: 4-bit fixed sequence (S0–S3), repeating every 4 primary epochs.
    *   **Tertiary**: Weil-1500 code, 1 chip per 4 primary epochs (20 ms duration).
    
    The combiner handles the phasing logic (``LSIS-TBD-2001`` interim assumption: phase 0) and outputs a binary chip stream.

3.  **Modulation (C2)**:
    Binary chips ``{0, 1}`` are mapped to bipolar baseband samples ``{+1, -1}``.
    
    *   **AFS-I**: Modulated with navigation data (currently fixed +1 data symbol for C1-C4 tests).
    *   **AFS-Q**: Pilot channel, no data modulation (data symbol always +1).

4.  **IQ Multiplexing (C4)**:
    The two channels are combined into a single complex baseband stream at the higher chip rate (5.115 Mcps).
    
    *   **AFS-I (1.023 Mcps)**: Upsampled by 5x (sample-and-hold) to match the output rate.
    *   **AFS-Q (5.115 Mcps)**: Passed through directly.
    *   **Output**: Interleaved ``int16`` array ``[I0, Q0, I1, Q1, ...]``.

Component Status
----------------

.. list-table::
   :header-rows: 1
   :widths: 15 30 20

   * - Part
     - Description
     - Status
   * - C1
     - Code loader (Gold-2046, Weil-10230, Weil-1500)
     - Complete
   * - C2
     - BPSK modulator (AFS-I BPSK(1) + AFS-Q BPSK(5))
     - Complete
   * - C3
     - Matched-code combiner (interim mapping + explicit assignment API)
     - Complete
   * - C4
     - IQ multiplexer (5.115 MSPS, normalized-amplitude baseband)
     - Complete
