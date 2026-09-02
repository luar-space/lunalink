C++ API Reference
=================

The C++ core, rendered from the headers by Doxygen and Breathe.  This is the
implementation the Python bindings wrap: each entry under *Low-Level Bindings*
in :doc:`api` corresponds to one call here.

For the interface *contract* — invariants, error semantics, HAL expectations
and the binary formats — see :doc:`signal/icd`.  This page is the
signature-level reference; the ICD is the agreement.

.. note::

   Generated from ``cpp/include/lunalink`` by ``task docs-doxygen``, which
   ``task docs-build`` runs first.  The hardening attributes
   (``LUNALINK_PACKED_ATTR``, ``LUNALINK_LUT_SECTION``, ``NOCOVER``) are
   expanded away in the rendered signatures: they are compiler placement and
   coverage plumbing, not interface.

Where to look
-------------

Everything lives in ``lunalink::signal`` (with the CRC-32 helpers in
``lunalink::core``), so the full reference below is one alphabetical listing.
This table is the way in, by subsystem:

.. list-table::
   :header-rows: 1
   :widths: 24 76

   * - Subsystem
     - Start from
   * - **Spreading codes**
     - :cpp:class:`~lunalink::signal::GoldGenerator`,
       :cpp:func:`~lunalink::signal::weil_code`,
       :cpp:func:`~lunalink::signal::matched_code`,
       :cpp:struct:`~lunalink::signal::PrnId`
   * - **Forward error correction**
     - :cpp:func:`~lunalink::signal::bch_encode`,
       :cpp:func:`~lunalink::signal::bch_decode`,
       :cpp:func:`~lunalink::signal::ldpc_encode`,
       :cpp:func:`~lunalink::signal::ldpc_decode`,
       :cpp:func:`~lunalink::signal::block_interleave`,
       :cpp:func:`~lunalink::signal::crc24_calculate`
   * - **Navigation messages**
     - :cpp:func:`~lunalink::signal::frame_build`,
       :cpp:func:`~lunalink::signal::frame_decode`,
       :cpp:func:`~lunalink::signal::parse_sb2`
   * - **Signal generation and reception**
     - :cpp:func:`~lunalink::signal::generate_iq_frame`,
       :cpp:func:`~lunalink::signal::modulate_bpsk`,
       :cpp:func:`~lunalink::signal::multiplex_iq`,
       :cpp:class:`~lunalink::signal::SyncDetector`,
       :cpp:class:`~lunalink::signal::Fft`
   * - **Core services**
     - :cpp:func:`~lunalink::signal::compute_sise`,
       :cpp:class:`~lunalink::signal::CheckedRange`,
       :cpp:class:`~lunalink::signal::ITelemetryHal`
   * - **Platform integrity**
     - :cpp:func:`~lunalink::signal::sci_header_verify`,
       :cpp:class:`~lunalink::signal::Watchdog`,
       :cpp:class:`~lunalink::signal::CfiGuard`

The safety primitives in the last two rows are the ones the hardening patterns
are built from; :doc:`assurance/mission_assurance` explains why each exists and
what failure it defends against.

----

``lunalink::signal``
--------------------

.. doxygennamespace:: lunalink::signal
   :members:

``lunalink::core``
------------------

.. doxygennamespace:: lunalink::core
   :members:
