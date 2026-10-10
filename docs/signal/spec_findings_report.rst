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
can make different choices and fail to interoperate.  Every ``{LSIS-TBW-*}`` /
``{LSIS-TBC-*}`` / ``{LSIS-TBD-*}`` marker in LSIS V1.0 (15 TBW, 3 TBC,
1 TBD) is dispositioned in :doc:`shall_inventory`.

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
    complete bit-level field definitions — the remaining 13 are marked TBW.
*   **Conflict**: Total frame construction (GD3.1) is blocked without bit-level
    definitions for these types.
*   **Proposed Resolution**: LunaLink utilizes the **LunaLink SISICD extension**
    to provide draft bit-layouts for TBW types (e.g., G3 High-Precision Clock).
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
    §Spec Finding SF-005) documents the assumption; the generated Gateway 6
    interop vectors carry it in a per-file ``interop_notes`` block so a
    consuming implementation cannot adopt the layout unknowingly.

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
