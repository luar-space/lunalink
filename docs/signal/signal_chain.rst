Signal Chain (GD1-GD4)
======================

The signal generation pipeline follows the LSIS V1.0 Augmented Forward Signal (AFS) specification: modular, flight-hardened components that turn navigation data into baseband I/Q samples.

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
             │  ◀── Nav Data Symbols    │
             ▼      (500 sps, C8 frame) │
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
    A lookup into the static tables (``prn_table_*.cpp``) returns the full chip sequences for the Primary (Gold-2046, Weil-10230) and Tertiary (Weil-1500) codes.  PRN ID validation is TMR-protected; storage is packed.

2.  **Matched-Code Combination (GD1.2)**:
    For the AFS-Q pilot channel, three components are XORed together to form the final ranging code:

    *   **Primary**: Weil-10230 code (10230 chips, 2 ms epoch).
    *   **Secondary**: 4-bit fixed sequence (S0–S3), repeating every 4 primary epochs.
    *   **Tertiary**: Weil-1500 code, 1 chip per 4 primary epochs (8 ms duration).

    The combiner handles phasing logic and enforces ``CheckedRange`` constraints on offsets.

3.  **Modulation (GD4.1)**:
    Binary chips ``{0, 1}`` are mapped to bipolar baseband samples ``{+1, -1}`` using branchless arithmetic.

    *   **AFS-I**: Modulated with navigation data symbols.
    *   **AFS-Q**: Pilot channel, no data modulation (data symbol always +1).

4.  **IQ Multiplexing (GD4.2)**:
    The two channels are combined into a single complex baseband stream at the higher chip rate (5.115 Mcps) with aliasing protection and watchdog liveness ticks (work-in-progress counter updates for the software watchdog).

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
   * - C5-E
     - BCH(51,8) Header Encoder
     - Implemented [Hardened]
   * - C5-D
     - BCH(51,8) ML Correlation Decoder
     - Implemented [Hardened]
   * - C6
     - LDPC Encoders (SF2 2400,1200; SF3/SF4 1740,870)
     - Implemented [Hardened]
   * - C7
     - LDPC Decoder (layered sum-product belief propagation)
     - Implemented [Hardened]
   * - C8
     - Navigation Frame Builder (sync, SB1, 60×98 block interleaving)
     - Implemented
   * - C9
     - Navigation Message Parsing (FID/TOI, WN/ITOW, ToT)
     - Implemented

The GD-prefixed rows are the gateway deliverables this page's diagram covers
in detail; the C-prefixed rows are the encoding/framing components of the
full chain (IDs per :doc:`../roadmap`), which feed the ``Nav Data Symbols``
input above.
