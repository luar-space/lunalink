Signal-In-Space Interface Control Document (SISICD)
===================================================

The SISICD proposes bit-level payload layouts for the navigation message
types whose content LSIS V1.0 leaves "To Be Written" (TBW) — 11 LunaNet-1.0 message types carry explicit
``{LSIS-TBW-*}`` content markers; see the "Messages with TBW content" list
in :doc:`shall_inventory` for the authoritative enumeration.

The message types below use these LunaLink-defined field layouts.  Every field
packs big-endian, MSB first.

TBW Message Index
-----------------

The 11 LunaNet-1.0 message types whose content LSIS V1.0 marks TBW, per
the spec's §2.5 message sections (authoritative list with the
``{LSIS-TBW-*}`` markers: :doc:`shall_inventory`):

.. list-table::
   :header-rows: 1
   :widths: 15 20 25 40

   * - Message
     - Spec section
     - TBW marker
     - SISICD status
   * - MSG-G1
     - §2.5.1
     - {LSIS-TBW-2004}
     - Raw bit-slice passthrough
   * - MSG-G2
     - §2.5.2
     - {LSIS-TBW-2005}
     - Raw bit-slice passthrough
   * - MSG-G4
     - §2.5.3
     - {LSIS-TBW-2006}
     - Raw bit-slice passthrough
   * - MSG-G5
     - §2.5.4
     - {LSIS-TBW-2007}
     - Raw bit-slice passthrough
   * - MSG-G10
     - §2.5.6
     - {LSIS-TBW-2008}
     - Raw bit-slice passthrough
   * - MSG-G11
     - §2.5.7
     - {LSIS-TBW-2009}
     - Raw bit-slice passthrough
   * - MSG-G17
     - §2.5.10
     - {LSIS-TBW-2010}
     - Raw bit-slice passthrough
   * - MSG-G28
     - §2.5.18
     - {LSIS-TBW-2011}
     - Raw bit-slice passthrough
   * - MSG-G30
     - §2.5.20
     - {LSIS-TBW-2012}
     - Raw bit-slice passthrough
   * - MSG-G31
     - §2.5.21
     - {LSIS-TBW-2013}
     - Raw bit-slice passthrough
   * - MSG-G32
     - §2.5.22
     - {LSIS-TBW-2014}
     - Raw bit-slice passthrough

(MSG-G14, G15, S19, G22–G25, G27, and G29 are defined outside the
LunaNet-1.0 scope and are not part of this index.)

LunaLink deliberately does **not** invent field layouts for TBW content:
the decode chain surfaces each TBW message's payload as a raw bit-slice
with an explicit ``_lsis_ref`` marker (see the parsed-data caveat in
:doc:`../test_vectors`), so implementations can byte-compare payloads
without committing to bit positions the spec has not yet pinned.

Proposed Bit-Layout (Common Header)
-----------------------------------

The sub-block sizes themselves are fixed by LSIS V1.0 (Table 14): before
rate-1/2 LDPC encoding, SB2 is 1200 bits (1176 data + 24-bit CRC-24) and
SB3/SB4 are 870 bits each (846 data + 24-bit CRC-24).  The SISICD does not
change those containers — it defines the layout of the *message payload*
carried inside a sub-block's message-data field.  Each SISICD-defined
message payload uses the common structure below, placed at the start of the
sub-block's data field (any remaining data bits carry the spec-mandated
alternating spare pattern, LSIS-300):

.. code-block:: text

   MSG_Header (16 bits):
     [15:10] Message ID
     [9:8]   Sub-block sequence number (00=SB2, 01=SB3, 10=SB4)
     [7:0]   Reserved (00h)

   MSG_Payload (message-specific length):
     Message-specific bit fields (defined in subsequent sections)

   The sub-block's CRC-24 (part of the 1200/870-bit container, per LSIS
   §2.4.3.1.3) covers all data bits including the SISICD message.

Illustrative Example of the Header Convention
---------------------------------------------

The sketch below shows how a future spec revision (or an interop working
group) could pin a payload using the common header above.  It is a
**worked example only** — MSG-G3's content is defined by the spec, and
LunaLink does not transmit or parse this layout.

Example: a high-precision clock-model payload
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Bits
     - Field
     - Description
   * - 64
     - ``a_0``
     - Clock bias (64-bit float, IEEE-754)
   * - 64
     - ``a_1``
     - Clock drift (64-bit float, IEEE-754)
   * - 32
     - ``t_oc``
     - Reference time (32-bit uint)

Concrete layout proposals for the TBW messages are a matter for the
LNIS working group; LunaLink's proposed process for that is in
:doc:`spec_findings_report`.
