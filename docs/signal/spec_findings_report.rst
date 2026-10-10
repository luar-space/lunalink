Proposed Standards Resolutions
==============================

.. rubric:: LUNALINK-SPEC-FINDINGS-001

Implementation assumptions and proposed resolutions for the ambiguities found
in the **LunaNet Signal-In-Space Recommended Standard** (LSIS V1.0, 29 January
2025).  Without them, two independent implementations cannot interoperate
deterministically.

Each finding carries a stable ``SF-NNN`` identifier that the rest of the
documentation cites.  The register below covers the ambiguities that are
**interoperability-critical** — those where two conformant implementations
can make different choices and fail to interoperate.  LSIS V1.0 lists 46
open items in its Table ZZ-1 (16 TBW, 25 TBC and 5 TBD).  Every one is
dispositioned in the table of `All open items in LSIS V1.0`_ below.

Register
--------

.. list-table::
   :header-rows: 1
   :widths: 10 40 20 30

   * - ID
     - Finding
     - Spec marker
     - Interop severity
   * - :ref:`SF-001 <sf-001>`
     - Matched-code phasing for AFS-Q is TBD
     - ``LSIS-TBD-2001``
     - High — no acquisition lock without agreement
   * - :ref:`SF-002 <sf-002>`
     - Navigation message content is TBW for 13 of 14 LNIS V1.0 types
     - ``LSIS-TBW-*`` (Table 21)
     - High — payloads opaque beyond the type field
   * - :ref:`SF-003 <sf-003>`
     - No digital baseband interchange format is defined
     - (gap, not marked)
     - Medium — blocks SDR cross-validation
   * - :ref:`SF-004 <sf-004>`
     - SB3/SB4 type field width is "4 or 6" bits
     - ``LSIS-TBC-2023``, ``LSIS-TBC-2024``
     - High — all downstream field offsets shift
   * - :ref:`SF-005 <sf-005>`
     - SB2 field layout (WN/ITOW bit offsets) is unspecified
     - ``LSIS-TBW-*`` (Table 22)
     - High — time recovery fails silently

----

.. _sf-001:

SF-001 — Matched-Code Phasing for AFS-Q
----------------------------------------

*   **Observation**: §2.3.5.4 notes that matched-code assignment and tertiary
    phasing are TBD (``LSIS-TBD-2001``).
*   **Conflict**: Independent implementations cannot achieve lock without
    synchronized phasing logic.
*   **Proposed Resolution**: LunaLink implements the **Interim Phasing Model**
    (Phase=0) for Service Provider Nodes 1–12.  Future phasing offsets belong
    in the Matched-Code Identifier (MCID) mapping table, where a receiver can
    load them without a code change.

.. _sf-002:

SF-002 — Navigation Message Structure (SB2–SB4)
------------------------------------------------

*   **Observation**: Table 21 defines 23 message types; 14 are designated
    LNIS V1.0. Of those 14, only MSG-G8 (Time of Transmission) includes
    complete bit-level field definitions.  Of the other 13, eleven are marked
    TBW, and MSG-G3 and MSG-S20 carry TBD items.
*   **Conflict**: Total frame construction is blocked without bit-level
    definitions for these types.
*   **Proposed Resolution**: LunaLink utilizes the **LunaLink SISICD extension**
    to provide draft bit-layouts for the TBW types (:doc:`sisicd`).
    This allows for full-frame validation until the formal SISICD is published.

.. _sf-003:

SF-003 — Baseband Digital Interchange Format
----------------------------------------------

*   **Observation**: The standard defines the RF waveform but not the digital
    baseband format for SDR interoperability.
*   **Proposed Resolution**: We propose a standard for **Interleaved Signed 16-bit
    IQ (int16)** pairs at the AFS-Q chip rate (5.115 MSPS) as the primary
    digital interchange format for hardware-in-the-loop validation.  The
    concrete container LunaLink ships — a 128-byte ``LSISIQ`` header plus
    interleaved samples — is specified in :doc:`/test_vectors`.

.. _sf-004:

SF-004 — SB3/SB4 Type Field Width (TBC)
-----------------------------------------

*   **Observation**: Tables 19 and 20 give the SB3/SB4 type field width as
    "4 or 6" bits, marked ``{LSIS-TBC-2023}`` / ``{LSIS-TBC-2024}``.
*   **Conflict**: The two choices are not compatible. A 4-bit field cannot
    represent the MSG IDs up to 32 that Table 21 defines, and every field
    offset after the type field shifts by 2 bits — so a 4-bit and a 6-bit
    implementation disagree on the entire remainder of the subframe, not just
    on the type.
*   **Proposed Resolution**: LunaLink uses **6 bits**, because Table 21
    defines MSG IDs up to 32 and 6 bits is the only width that can encode
    them. We propose the next LSIS revision resolve ``LSIS-TBC-2023`` and
    ``LSIS-TBC-2024`` to 6 bits for consistency with Table 21.
*   **Verification**: :doc:`/verification/message_parsing` (§Interoperability Risks) documents the
    failure mode; ``tests/test_interop_frames.py`` pins the LunaLink choice.

.. _sf-005:

SF-005 — SB2 Field Layout (TBW)
---------------------------------

*   **Observation**: Table 22 defines the SB2 Week Number (13 bits) and
    Interval Time of Week (9 bits) field *widths*, but not their bit
    *offsets* within the subframe. The remaining 1154 SB2 data bits are TBW.
*   **Conflict**: Time-of-transmission recovery is the one fully specified
    message (MSG-G8), yet a receiver cannot locate its inputs. Two
    implementations that disagree on the offsets both decode without error
    and produce different times — the failure is silent, which is worse than
    a decode failure.
*   **Proposed Resolution**: LunaLink places **WN at bit offset 0 and ITOW at
    bit offset 13**, MSB-first per LSIS-320. This is a reasonable default but
    is **not normative**. We propose the next LSIS revision fix the MSG-G4,
    MSG-G2 and MSG-G8 field ordering and offsets explicitly.
*   **Verification**: :doc:`/verification/message_parsing` (§Interoperability Risks and
    §Spec Finding SF-005) documents the assumption; the vectors generated by
    ``scripts/generate_gateway6_vectors.py`` carry it in a per-file ``interop_notes`` block so a
    consuming implementation cannot adopt the layout unknowingly.

----

.. _open-items:

All open items in LSIS V1.0
---------------------------

Table ZZ-1 of LSIS V1.0 lists 46 open items: 16 marked *to be written*, 25
marked *to be confirmed* and 5 marked *to be determined*.  The table below
gives each one with what LunaLink does about it.  The designation, section,
requirement and title are taken from Table ZZ-1.  Where an item attaches to
a requirement, the disposition follows that requirement's entry in
:doc:`shall_inventory`.

Most of the items marked *to be confirmed* are tolerances on radio-frequency
hardware and do not affect a baseband implementation.  The ones that do
affect it are the code definitions (LSIS-TBC-2020), the code assignments
(LSIS-TBD-2001), the synchronisation pattern (LSIS-TBC-2021), the subframe
bit allocations (LSIS-TBC-2022 to 2024) and the message contents
(LSIS-TBW-2004 to 2014).

.. list-table::
   :header-rows: 1
   :widths: 14 12 14 22 38

   * - Item
     - Section
     - Requirement
     - Title
     - What LunaLink does
   * - LSIS-TBD-1001
     - 1
     - —
     - SISICD distribution
     - Programme matter; nothing to implement.
   * - LSIS-TBC-2001
     - 2.2.3
     - LSIS-001
     - SISE position error
     - Limit of 40 m used as given.
   * - LSIS-TBC-2002
     - 2.2.3
     - LSIS-001
     - SISE position period of performance
     - Not fixed in the code: the caller supplies the error series.
   * - LSIS-TBC-2003
     - 2.2.3
     - LSIS-002
     - SISE velocity error
     - Limit of 0.01 m/s used as given.
   * - LSIS-TBC-2004
     - 2.2.3
     - LSIS-002
     - SISE velocity period of performance
     - Not fixed in the code: the caller supplies the error series.
   * - LSIS-TBW-2001
     - 2.2.3
     - LSIS-006
     - SISE position prediction accuracy
     - Content not written; not implemented.
   * - LSIS-TBW-2002
     - 2.2.3
     - LSIS-007
     - SISE velocity prediction accuracy
     - Content not written; not implemented.
   * - LSIS-TBC-2005
     - 2.3.1.2
     - LSIS-050
     - RHCP axial ratio
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2006
     - 2.3.1.3
     - LSIS-060
     - AFS phase noise
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2007
     - 2.3.1.4
     - LSIS-070
     - Maximum in-band spurious emissions
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBW-2003
     - 2.3.1.4
     - LSIS-080
     - Maximum out-of-band emissions
     - Content not written, and a radio-frequency property.
   * - LSIS-TBC-2008
     - 2.3.1.5
     - LSIS-090
     - AFS reference bandwidth
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2009
     - 2.3.1.5
     - LSIS-100
     - Correlation loss
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2010
     - 2.3.1.6
     - LSIS-103
     - AFS-I power percentage tolerance
     - The tolerance applies to transmitter hardware.  LunaLink splits power equally between the two channels.
   * - LSIS-TBC-2011
     - 2.3.1.6
     - LSIS-103
     - AFS-Q power percentage tolerance
     - As LSIS-TBC-2010.
   * - LSIS-TBC-2012
     - 2.3.1.7
     - LSIS-110
     - Minimum received power in the service volume
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2013
     - 2.3.1.7
     - LSIS-110
     - Maximum received power in the service volume
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2014
     - 2.3.1.8
     - LSIS-120
     - Maximum received power outside the service volume
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2015
     - 2.3.1.8
     - LSIS-120
     - Applicability of LSIS-120
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2016
     - 2.3.4
     - LSIS-160
     - Primary code to data coherency
     - The tolerance applies to transmitter hardware.  LunaLink generates the components aligned to the sample at baseband.
   * - LSIS-TBC-2017
     - 2.3.4
     - LSIS-170
     - Pilot primary to secondary code coherency
     - The tolerance applies to transmitter hardware.  LunaLink generates the components aligned to the sample at baseband.
   * - LSIS-TBC-2017a
     - 2.3.4
     - LSIS-171
     - Pilot secondary to tertiary code coherency
     - The tolerance applies to transmitter hardware.  LunaLink generates the components aligned to the sample at baseband.
   * - LSIS-TBC-2018
     - 2.3.4
     - LSIS-180
     - Data code to pilot code coherency
     - The tolerance applies to transmitter hardware.  LunaLink generates the components aligned to the sample at baseband.
   * - LSIS-TBC-2019
     - 2.3.4
     - LSIS-190
     - Code to carrier coherency
     - Radio-frequency property; out of scope for a baseband implementation.
   * - LSIS-TBC-2020
     - 2.3.5.3.2
     - LSIS-222
     - AFS-Q primary code definition
     - Codes implemented as given in Appendix D and Annex 3.  A change to the Weil or insertion indices would change every AFS-Q code.
   * - LSIS-TBD-2001
     - 2.3.5.4
     - LSIS-260
     - Code assignments per LNSP node
     - Interim assignments of Table 11 for PRN 1 to 12, with a tertiary phase offset of 0 (:ref:`SF-001 <sf-001>`).
   * - LSIS-TBC-2021
     - 2.4.1.2
     - LSIS-330
     - Synchronisation pattern
     - Pattern implemented as given.  A change would stop frame synchronisation between old and new implementations.
   * - LSIS-TBC-2022
     - 2.4.3.1.6
     - LSIS-FID0-550
     - Subframe 2 spare bits
     - The standard gives no quantity.  LunaLink keeps 26 spare bits (SB2 bits
       1150 to 1175), filled with the alternating pattern.
   * - LSIS-TBC-2023
     - 2.4.3.1.7
     - LSIS-FID0-560
     - Subframe 3 bit allocation
     - Type field of 6 bits (:ref:`SF-004 <sf-004>`).
   * - LSIS-TBC-2024
     - 2.4.3.1.8
     - LSIS-FID0-600
     - Subframe 4 bit allocation
     - Type field of 6 bits (:ref:`SF-004 <sf-004>`).
   * - LSIS-TBD-2002
     - 2.4.3.1.9
     - Table 21
     - MSG-G3 subframe allocation
     - No allocation assumed.  The message is identified by its type field and its payload carried as raw bits.
   * - LSIS-TBW-2004
     - 2.5.1
     - —
     - MSG-G1 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2005
     - 2.5.2
     - —
     - MSG-G2 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2006
     - 2.5.3
     - —
     - MSG-G4 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2007
     - 2.5.4
     - —
     - MSG-G5 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBD-2003
     - 2.5.5
     - LSIS-720
     - LRT start epoch
     - No absolute epoch assumed.  Time of transmission is computed in seconds from the epoch, whatever it is defined to be.
   * - LSIS-TBW-2008
     - 2.5.6
     - —
     - MSG-G10 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2009
     - 2.5.7
     - —
     - MSG-G11 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2010
     - 2.5.10
     - —
     - MSG-G17 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBD-2004
     - 2.5.12
     - —
     - MSG-S20 bit allocation
     - No allocation assumed; the payload is carried as raw bits.
   * - LSIS-TBW-2011
     - 2.5.18
     - —
     - MSG-G28 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2012
     - 2.5.20
     - —
     - MSG-G30 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2013
     - 2.5.21
     - —
     - MSG-G31 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2014
     - 2.5.22
     - —
     - MSG-G32 specification
     - Content not written.  The payload is carried as raw bits; a proposed layout is in :doc:`sisicd` (:ref:`SF-002 <sf-002>`).
   * - LSIS-TBW-2015
     - Appendix G
     - —
     - User algorithms
     - Content not written; nothing to implement.
   * - LSIS-TBW-2016
     - Appendix I
     - —
     - Reference AFS link budget
     - Content not written, and a radio-frequency matter.

----

Recommendation to the LNIS Working Group
-----------------------------------------

In priority order for interoperability: resolve **SF-005** (SB2 field
offsets) and **SF-004** (type field width) first — both are cheap
editorial decisions that unblock cross-implementation decoding today.
**SF-001** (matched-code phasing) follows, as it gates AFS-Q acquisition
between independent receivers. **SF-002** is the largest body of work and
can proceed incrementally, message type by message type. **SF-003** is a
proposal for new material rather than a defect in the existing text.
