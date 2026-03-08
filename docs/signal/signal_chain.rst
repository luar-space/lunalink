Signal Chain (GD1-GD4)
======================

The LunaLink signal generation pipeline follows the LSIS V1.0 Augmented Forward Signal (AFS) specification. It is composed of modular, flight-hardened components that transform navigation data into baseband I/Q samples.

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
    │  GD1.1 Code Ldr  │     │  Tertiary Code         │
    │  (Gold-2046)     │     │  (Weil-1500)           │
    └────────┬────────┘     └──────────┬───────────┘
             │                          │
             │                          │
             ▼                          │
    ┌─────────────────┐                 │
    │  GD4.1 BPSK(1)  │                 │
    │  Modulator      │                 │
    │                 │                 │
    │  1.023 Mcps     │                 │
    │  int8 {-1, +1}  │                 │
    └────────┬────────┘                 │
             │                          │
             │                          │
             │                          ▼
             │                ┌─────────────────┐
             │                │  GD1.2 Matched  │
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
             │                │  GD4.1 BPSK(5)  │
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
    │              GD4.2 IQ Multiplexer            │
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

1.  **Code Loading (GD1.1)**:
    Static tables (``prn_table_*.cpp``) are queried to retrieve the full chip sequences for the Primary (Gold-2046, Weil-10230) and Tertiary (Weil-1500) codes. Includes TMR-protected PRN ID validation and packed storage.

2.  **Matched-Code Combination (GD1.2)**:
    For the AFS-Q pilot channel, three components are XORed together to form the final ranging code:
    
    *   **Primary**: Weil-10230 code (10230 chips, 2 ms epoch).
    *   **Secondary**: 4-bit fixed sequence (S0–S3), repeating every 4 primary epochs.
    *   **Tertiary**: Weil-1500 code, 1 chip per 4 primary epochs (20 ms duration).
    
    The combiner handles phasing logic and enforces ``CheckedRange`` constraints on offsets.

3.  **Modulation (GD4.1)**:
    Binary chips ``{0, 1}`` are mapped to bipolar baseband samples ``{+1, -1}`` using branchless arithmetic.
    
    *   **AFS-I**: Modulated with navigation data symbols.
    *   **AFS-Q**: Pilot channel, no data modulation (data symbol always +1).

4.  **IQ Multiplexing (GD4.2)**:
    The two channels are combined into a single complex baseband stream at the higher chip rate (5.115 Mcps) with aliasing protection and WIP signaling.

Component Status
----------------

.. list-table::
   :header-rows: 1
   :widths: 15 30 20

   * - ID
     - Description
     - Status
   * - GD1.1
     - Spreading Code Loader (Gold, Weil, Legendre)
     - Implemented [Hardened]
   * - GD1.2
     - Matched-code Combiner (Tiered assembly)
     - Implemented [Hardened]
   * - GD4.1
     - BPSK Modulator (I + Q)
     - Implemented [Hardened]
   * - GD4.2
     - IQ Multiplexer (5.115 MSPS)
     - Implemented [Hardened]
