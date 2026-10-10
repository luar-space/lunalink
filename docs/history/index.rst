Project History
===============

LunaLink was built between March and September 2026 for the ESA-CCSDS Protocol
Development Outreach Competition, on the LSIS-AFS track, and won it.

The documents in this section were written for that competition. The
submission report and the evaluation guide are preserved unchanged. They are
the most complete account of what existed at submission and how each claim
could be checked, which makes them useful reading and a poor starting point.
Figures, counts and open items in this section are frozen at September 2026
and are not maintained.

The competition divided the work into nine gateways. The guides for gateways
1 to 6 are maintained and now live under :doc:`/verifying`, one per block of
the signal chain. The guides for gateways 0, 7 and 8 described the
submission itself and are kept here as a record. Where a page cited in this
section has since moved, its link points to the new location.

Current information lives elsewhere: :doc:`/quickstart` to get running,
:doc:`/roadmap` for what is done and what is not, :doc:`/interop_results` for
the interoperability evidence, and ``docs/assurance/`` for the assurance case.

.. toctree::
   :maxdepth: 2

   competition_submission
   evaluation_guide
   gateway0
   gateway7
   gateway8

Why this is kept
----------------

A reference implementation is only as trustworthy as its record. These
documents show what was claimed, to whom, and on what date, against an
implementation whose git history is public. Anyone who wants to check whether
the project's claims have drifted since can diff the claims against the code at
the ``v1.0.0`` tag.

The competition result is a statement about the work at a point in time. It is
not a designation of official status by NASA, ESA or CCSDS, and nothing in
these documents should be read as implying one. The project's relationship to
the standard and its authors is set out in ``GOVERNANCE.md`` in the repository
root.
