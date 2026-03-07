Proposed Standards Resolutions
==============================

This document formalizes implementation assumptions and proposed resolutions 
for ambiguities identified in the **LunaNet Signal-In-Space Recommended Standard** 
(LSIS V1.0, 29 January 2025). These resolutions ensure deterministic 
interoperability between independent implementations.

1. BCH(51,8) Polynomial Discrepancy
-----------------------------------

*   **Observation**: §2.4.2.1 specifies the generator polynomial as ``763`` (octal), 
    whereas Figure 7 explicitly diagrams :math:`1+X^3+X^4+X^5+X^6+X^7+X^8` (771 octal).
*   **Conflict**: The provided test vector in Figure 8 (SB1=0x045) is only 
    reproducible using the Figure 7 polynomial.
*   **Proposed Resolution**: LunaLink adopts the **Figure 7 polynomial** as the 
    ground truth. We recommend the standard text be updated from ``763`` to ``771``.

2. Matched-Code Phasing for AFS-Q
---------------------------------

*   **Observation**: §2.3.5.4 notes that matched-code assignment and tertiary 
    phasing are TBD (``LSIS-TBD-2001``).
*   **Conflict**: Independent implementations cannot achieve lock without 
    synchronized phasing logic.
*   **Proposed Resolution**: LunaLink implements the **Interim Phasing Model** 
    (Phase=0) for Service Provider Nodes 1-12. We propose that future phasing 
    offsets be integrated into the Matched-Code Identifier (MCID) mapping table 
    to maintain receiver synchronization.

3. Navigation Message Structure (SB2–SB4)
-----------------------------------------

*   **Observation**: The standard defines 22 message types, but 14 are marked 
    as "To Be Written" (TBW).
*   **Conflict**: Total frame construction (C8) is blocked without bit-level 
    definitions for these types.
*   **Proposed Resolution**: LunaLink utilizes the **LunaLink SISICD extension** 
    to provide draft bit-layouts for TBW types (e.g., G3 High-Precision Clock). 
    This allows for full-frame validation until the formal SISICD is published.

4. Baseband Digital Interchange Format
--------------------------------------

*   **Observation**: The standard defines the RF waveform but not the digital 
    baseband format for SDR interoperability.
*   **Proposed Resolution**: We propose a standard for **Interleaved Signed 16-bit 
    IQ (int16)** pairs at the AFS-Q chip rate (5.115 MSPS) as the primary 
    digital interchange format for hardware-in-the-loop validation.
