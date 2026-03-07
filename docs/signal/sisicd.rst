Signal-In-Space Interface Control Document (SISICD)
===================================================

This document (SISICD) defines the bit-level layouts for the navigation 
message sub-blocks (SB2, SB3, SB4) for the 14 message types marked as 
"To Be Written" (TBW) in the LSIS V1.0 specification.

The following message types are currently implemented using these LunaLink-defined 
field layouts. All fields are packed Big-Endian (MSB-first).

TBW Message Index
-----------------

.. list-table::
   :header-rows: 1
   :widths: 10 30 60

   * - ID
     - Message Type
     - SISICD Definition Status
   * - G3
     - Clock Model Type 3
     - Draft (TBD)
   * - G6
     - Orbit Model Type 2
     - Draft (TBD)
   * - G7
     - Orbit Model Type 3
     - Draft (TBD)
   * - G9-G21
     - Reserved / Misc
     - Draft (TBD)

Proposed Bit-Layout (Common Header)
-----------------------------------

Each sub-block (SB2, SB3, SB4) consists of 200 bits before LDPC encoding.

.. code-block:: text

   SB_Header (16 bits):
     [15:10] Message ID
     [9:8]   Sub-block sequence number (00=SB2, 01=SB3, 10=SB4)
     [7:0]   Reserved (00h)

   SB_Payload (160 bits):
     Message-specific bit fields (defined in subsequent sections)

   SB_CRC (24 bits):
     CRC-24 computed over SB_Header + SB_Payload

Message Type Layouts (SISICD-defined)
-------------------------------------

G3: Clock Model Type 3 (High Precision)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

(More message types will be added as they are implemented in Slice V3/V4).
