Signal Chain — Transmitter
==========================

This diagram shows the AFS transmit signal chain from PRN tables to baseband
IQ output. Part labels (C1–C4) correspond to the implementation parts defined
in the shaping document.

Block Diagram
-------------

.. code-block:: text

                                   AFS-I (Data Channel)
                                   ────────────────────

    ┌─────────────────┐     ┌──────────────────────┐
    │ PRN Hex Tables   │     │  Navigation Message   │
    │ (Gold-2046)      │     │  (SB1–SB4 symbols)    │
    └────────┬────────┘     └──────────┬───────────┘
             │                          │
             ▼                          ▼
    ┌─────────────────┐     ┌──────────────────────┐
    │  C1 Code Loader  │     │  Data symbol (±1)     │
    │  gold_prn(id)    │     │  from frame builder   │
    │                  │     │                       │
    │  uint8 chips     │     │  int8_t               │
    │  {0, 1}          │     │  {−1, +1}             │
    └────────┬────────┘     └──────────┬───────────┘
             │                          │
             └──────────┬───────────────┘
                        │
                        ▼
               ┌─────────────────┐
               │  C2 BPSK(1)     │
               │  Modulator      │
               │                 │
               │  chip mapping:  │
               │  0 → +1         │
               │  1 → −1         │
               │  × data symbol  │
               │                 │
               │  1.023 Mcps     │
               │  int8_t {−1,+1} │
               └────────┬────────┘
                        │
                        ▼

                                   AFS-Q (Pilot Channel)
                                   ─────────────────────

    ┌─────────────────┐     ┌──────────────────────┐
    │ PRN Hex Tables   │     │  Secondary Codes      │
    │ (Weil-10230)     │     │  S0–S3 (4-bit)        │
    └────────┬────────┘     └──────────┬───────────┘
             │                          │
             ▼                          ▼
    ┌─────────────────┐     ┌──────────────────────┐
    │  C1 Code Loader  │     │  Tertiary Code         │
    │  (Weil-10230 +   │     │  (Weil-1500)           │
    │   Weil-1500)     │     └──────────┬───────────┘
    └────────┬────────┘                │
             │                          │
             └──────────┬───────────────┘
                        │
                        ▼
               ┌─────────────────┐
               │  C3 Tiered Code │
               │  Combiner       │
               │                 │
               │  primary ⊕      │
               │  secondary ⊕    │
               │  tertiary       │
               │                 │
               │  5.115 Mcps     │
               │  uint8 {0, 1}   │
               └────────┬────────┘
                        │
                        ▼
               ┌─────────────────┐
               │  C2 BPSK(5)     │
               │  Modulator      │
               │                 │
               │  5.115 Mcps     │
               │  int8_t {−1,+1} │
               └────────┬────────┘
                        │
                        ▼

                                   IQ Combination
                                   ──────────────

               AFS-I ────────┐
               (1.023 Mcps)  │
                             ▼
                    ┌─────────────────┐
                    │  C4 IQ          │
                    │  Multiplexer    │
                    │                 │
                    │  50/50 power    │
                    │  I + jQ         │
                    │                 │
                    │  ≥10.23 MSPS    │
                    │  int16_t        │
                    └────────┬────────┘
               AFS-Q ────────┘
               (5.115 Mcps)

                        │
                        ▼
                  Baseband IQ
                  (complex)

Data Types Through the Chain
----------------------------

.. list-table::
   :header-rows: 1
   :widths: 25 25 25 25

   * - Stage
     - Part
     - Data Type
     - Rate
   * - PRN chips
     - C1
     - ``uint8_t`` {0, 1}
     - —
   * - AFS-I samples
     - C2
     - ``int8_t`` {−1, +1}
     - 1.023 Mcps
   * - AFS-Q composite chips
     - C3
     - ``uint8_t`` {0, 1}
     - 5.115 Mcps
   * - AFS-Q samples
     - C2
     - ``int8_t`` {−1, +1}
     - 5.115 Mcps
   * - Baseband IQ
     - C4
     - ``int16_t`` (I+Q composite)
     - ≥10.23 MSPS

Implementation Status
---------------------

.. list-table::
   :header-rows: 1
   :widths: 15 30 20

   * - Part
     - Description
     - Status
   * - C1
     - Code loader (Gold-2046)
     - V1 complete
   * - C2
     - BPSK modulator (AFS-I only)
     - V1 partial (AFS-I); AFS-Q in V2
   * - C3
     - Tiered code combiner
     - V2
   * - C4
     - IQ multiplexer
     - V2
