Development Roadmap
===================

This document outlines the phased development plan for LunaLink, tracking 
past achievements and upcoming milestones toward a complete, flight-ready 
AFS-I and AFS-Q Signal-In-Space (SIS) reference implementation.

Phased Implementation
---------------------

.. list-table::
   :header-rows: 1
   :widths: 10 60 30

   * - Phase
     - Focus Area
     - Status
   * - **V1**
     - AFS-I Baseband Generation (PRN codes, BPSK modulation)
     - ✅ Completed
   * - **V2**
     - Complete AFS Transmitter (AFS-I/Q Mux, SB1 Encoder, Docs)
     - ✅ Completed
   * - **V3**
     - Full Codec implementation (BCH/LDPC decoder, CRC-24, All MSG types)
     - 🏗️ In Progress
   * - **V4**
     - Validation & Interop (Golden test vectors, BER curves, HW-in-the-loop)
     - 📅 Scheduled
   * - **V5**
     - PNT Pipeline (Satellite constellation simulation, Position Fix)
     - 📅 Scheduled

Implementation Progress
-----------------------

### Component Traceability (V3 Focus)

.. list-table::
   :header-rows: 1
   :widths: 15 65 20

   * - ID
     - Component Description
     - Status
   * - **C1**
     - Spreading Code Loaders (Gold-2046, Weil-10230, Weil-1500)
     - ✅ Done
   * - **C2**
     - BPSK Modulators (AFS-I 1.023 MHz, AFS-Q 5.115 MHz)
     - ✅ Done
   * - **C3**
     - AFS-Q Matched-Code Combiner (Primary XOR Secondary XOR Tertiary)
     - ✅ Done
   * - **C4**
     - IQ Multiplexer (50/50 Power Sharing, Upsampling)
     - ✅ Done
   * - **C5-E**
     - BCH(51,8) Header Encoder
     - ✅ Done
   * - **C5-D**
     - BCH(51,8) ML Correlation Decoder (Radiation-Hardened)
     - ✅ Done
   * - **C6**
     - LDPC(1200,600) Encoder (5G NR Sub-matrices)
     - 🏗️ In Progress
   * - **C7**
     - LDPC(1200,600) Decoder (Min-Sum / Belief Propagation)
     - ❌ Not Started
   * - **C8**
     - Navigation Frame Builder (Sync, Interleaving, Scrambling)
     - ⚠️ Partial
   * - **C9**
     - Navigation Message Serializer (22 Message Types)
     - ❌ Not Started

Current Deliverables Status
---------------------------

*   **Standards Compliance**: 95% of M1 requirements traced and implemented.
*   **Architecture Documentation**: Complete design record for all core modules.
*   **Mission Assurance**: 100% Python and 98% C++ test coverage reached for core signal chain.
*   **Interoperability**: Test vector format defined; Golden set generation scheduled for V4.
