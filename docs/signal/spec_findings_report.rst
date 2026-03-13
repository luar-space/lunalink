Proposed Standards Resolutions
==============================

This document formalizes implementation assumptions and proposed resolutions 
for ambiguities identified in the **LunaNet Signal-In-Space Recommended Standard** 
(LSIS V1.0, 29 January 2025). These resolutions ensure deterministic 
interoperability between independent implementations.

1. Matched-Code Phasing for AFS-Q
---------------------------------

*   **Observation**: §2.3.5.4 notes that matched-code assignment and tertiary 
    phasing are TBD (``LSIS-TBD-2001``).
*   **Conflict**: Independent implementations cannot achieve lock without 
    synchronized phasing logic.
*   **Proposed Resolution**: LunaLink implements the **Interim Phasing Model** 
    (Phase=0) for Service Provider Nodes 1-12. We propose that future phasing 
    offsets be integrated into the Matched-Code Identifier (MCID) mapping table 
    to maintain receiver synchronization.

2. Navigation Message Structure (SB2–SB4)
-----------------------------------------

*   **Observation**: Table 21 defines 23 message types; 14 are designated
    LNIS V1.0. Of those 14, only MSG-G8 (Time of Transmission) includes
    complete bit-level field definitions — the remaining 13 are marked TBW.
*   **Conflict**: Total frame construction (GD3.1) is blocked without bit-level
    definitions for these types.
*   **Proposed Resolution**: LunaLink utilizes the **LunaLink SISICD extension** 
    to provide draft bit-layouts for TBW types (e.g., G3 High-Precision Clock). 
    This allows for full-frame validation until the formal SISICD is published.

3. Baseband Digital Interchange Format
--------------------------------------

*   **Observation**: The standard defines the RF waveform but not the digital 
    baseband format for SDR interoperability.
*   **Proposed Resolution**: We propose a standard for **Interleaved Signed 16-bit 
    IQ (int16)** pairs at the AFS-Q chip rate (5.115 MSPS) as the primary 
    digital interchange format for hardware-in-the-loop validation.

