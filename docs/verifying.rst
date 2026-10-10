Verifying this implementation
=============================

This page is for a reader who wants to check what LunaLink claims.  Each
section asks one question, names the pages that answer it, and gives the
command that reproduces the evidence.  Every command runs from the repository
root after ``task install`` (see :doc:`quickstart`).

You can also inspect the evidence without running anything.  The generated
artefacts are committed under ``docs/assurance/evidence/``:
``compliance_report.json``, ``traceability.json``, ``coverage_summary.json``,
``swifi_results.json`` and the JUnit XML output of the test, fault-injection,
timing and bit-error-rate suites.

Does it interoperate with other implementations?
------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Read
     - What it shows
   * - :doc:`interop_results`
     - Results at five levels against two independent implementations:
       LANS-AFS-SIM on the transmit side and PocketSDR-AFS on the receive
       side.  It includes the method and the known limitations.
   * - :doc:`interoperate`
     - How to run the same comparison against your own implementation.
   * - :doc:`test_vectors`
     - The file formats of the shared vectors.

.. code-block:: bash

   task interop            # levels 1 to 3, against LANS-AFS-SIM
   task interop-receiver   # levels 4 and 5, against PocketSDR-AFS

Does it conform to LSIS V1.0?
-----------------------------

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Read
     - What it shows
   * - :doc:`signal/shall_inventory`
     - Every normative identifier in LSIS V1.0, in the order of the
       standard's sections, with its disposition and the source file and test
       that implement it.
   * - :doc:`signal/pics_proforma`
     - The conformance statement, generated from the inventory.
   * - :doc:`signal/compliance_matrix`
     - Implementation status by block, including the receive side, which the
       standard does not specify with "shall" statements.
   * - :doc:`assurance/traceability_matrix`
     - Requirement tags in the source mapped to the tests that carry them.
       The machine-readable form is
       ``docs/assurance/evidence/traceability.json``.

.. code-block:: bash

   task traceability        # every requirement has a test; no test tag is orphaned
   task pics-proforma       # fails if the proforma and the inventory disagree
   task compliance-report   # grades each requirement from this run's test output

``task compliance-report`` runs the unit, integration, fault-injection, timing
and bit-error-rate suites first, and grades only from what that run produced.
It does not fall back to the committed evidence.

Where is the standard open, and what did LunaLink assume?
---------------------------------------------------------

LSIS V1.0 marks some content as to be written, to be confirmed or to be
determined.  Each marker is listed with its disposition in
:doc:`signal/shall_inventory`.

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Read
     - What it shows
   * - :doc:`signal/spec_findings_report`
     - Findings SF-001 to SF-005: the open items where two conformant
       implementations can choose differently and then fail to interoperate.
       Each gives the assumption LunaLink adopted and a proposed resolution.
   * - :doc:`signal/sisicd`
     - Proposed bit-level layouts for the message types whose payload the
       standard leaves to be written.

If you read a clause differently, open a `specification finding
<https://github.com/luar-space/lunalink/issues/new?template=spec_finding.yml>`__.

How is each block implemented and tested?
-----------------------------------------

One guide per block of the signal chain.  Each lists the parameters, the
source files, the public interface, the hardening applied, and the tests with
what each one checks.

.. list-table::
   :header-rows: 1
   :widths: 30 40 30

   * - Block
     - Guide
     - Source
   * - Spreading codes
     - :doc:`gateway1`
     - ``cpp/codes/``
   * - Forward error correction
     - :doc:`gateway2`
     - ``cpp/encoding/``
   * - Navigation message framing
     - :doc:`gateway3`
     - ``cpp/messages/frame.cpp``
   * - Baseband signal generation
     - :doc:`gateway4`
     - ``cpp/signal/``
   * - Frame synchronisation and decoding
     - :doc:`gateway5`
     - ``cpp/signal/sync_detector.cpp``, ``cpp/messages/frame_decode.cpp``
   * - Message parsing and time
     - :doc:`gateway6`
     - ``cpp/messages/message_parser.cpp``,
       ``cpp/messages/time_of_transmission.cpp``

.. code-block:: bash

   task test-cpp       # unit and integration tests, without the slow suites
   task test-py        # Python tests
   task integration    # end-to-end transmit and receive tests

:doc:`signal/architecture` and :doc:`signal/signal_chain` describe how the
blocks fit together, and :doc:`signal/icd` is the interface contract.

How well does it perform?
-------------------------

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Read
     - What it shows
   * - :doc:`signal/ldpc_performance`
     - Bit and frame error rates of the LDPC decoder against the target in
       the standard, with confidence intervals.
   * - :doc:`signal/ldpc_algo_card`
     - The shipped decoder measured against a reference decoder and the
       finite-length bound, with convergence, quantisation and error-floor
       results.
   * - :doc:`signal/bch_performance`
     - Frame error rate of the hard-decision and soft-decision BCH decoders.

.. code-block:: bash

   task performance      # timing benchmarks and bit-error-rate simulations
   task ldpc-algo-card   # regenerates the LDPC figures and tables

Can the evidence be trusted?
----------------------------

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Read
     - What it shows
   * - :doc:`assurance/tool_confidence`
     - The analysers and the coverage tool run against code with known
       defects, to show they report what they are relied on to report.
   * - :doc:`assurance/coverage_evidence`
     - Region, line, branch and MC/DC coverage per file.
   * - :doc:`assurance/coverage_exclusions`
     - Every excluded path and every file below the review trigger, with the
       reason.
   * - :doc:`assurance/fault_coverage`
     - The fault-injection scenarios and the test that exercises each.
       :doc:`assurance/fmea` is the analysis they come from.

.. code-block:: bash

   task validate-tidy       # clang-tidy catches the violations it is claimed to catch
   task validate-coverage   # llvm-cov reports known gaps correctly
   task coverage-cpp        # coverage, with thresholds
   task swifi               # fault injection
   task swifi-reconcile     # every scenario in the report has a test, and the reverse
   task check-doc-metrics   # figures quoted in the README match the evidence
   task validate-export     # the committed tree builds and passes, not just the working copy
   task ci-checks           # the whole gate, locally

The continuous-integration workflows are listed in :doc:`developing`.  They
include a build and test run on big-endian s390x and on arm64.

What is not done
----------------

These limits are stated here so that a reader does not have to discover them.

- LunaLink has not flown and has not been through independent verification
  and validation.  Conformance to ECSS Category B is self-assessed.  See
  :doc:`assurance/mission_assurance`.
- The receive-direction interoperability result compares subframe 2, the
  subframe PocketSDR-AFS logs in decoded form, on a signal generated at
  baseband.  It has not been repeated on a captured radio-frequency signal.
  Building PocketSDR-AFS needs one patch that renames a function; the patch
  changes no arithmetic.
- The receiver used for the level 3 cross-decode runs at one sample per chip.
  That is enough for the comparison and is not a receiver design for flight.
  See the known limitations in :doc:`interop_results`.
- The receiver has no delay-locked loop, so it does not compensate for clock
  drift.  The sample-clock-offset and phase-noise sweeps named in
  :doc:`signal/validation_plan` are not implemented.
- There is no positioning pipeline and no hardware-in-the-loop testing.
- Coverage passes with waivers, each recorded in
  :doc:`assurance/coverage_exclusions`.
- The target "SNR > 0 dB" in the performance requirement can be read two
  ways.  :doc:`signal/ldpc_performance` reports both.
- The development history in this repository is filtered from a larger
  private tree.  The README's Provenance section says how.

:doc:`roadmap` tracks the open work.

Terms you will meet
-------------------

LunaLink was first built for the ESA-CCSDS Protocol Development Outreach
Competition, whose rubric divided the work into nine *gateways*, numbered 0 to
8.  Some test names, script names and the pages under :doc:`history/index`
still use that word and deliverable identifiers of the form ``GD2.1``.
Gateways 1 to 6 correspond to the six blocks in the table above.

Three families of requirement identifier appear in the documentation:

- ``LSIS-nnn`` identifiers come from the standard and are listed in
  :doc:`signal/shall_inventory`.
- ``[LSIS-AFS-nnn]`` and ``[SW-SAF-nn]`` tags mark the source functions and
  tests that implement a requirement.  The traceability check works on these.
- ``CM-nnn`` identifiers number the rows of :doc:`signal/compliance_matrix`
  and are used nowhere else.

A ``CM`` row and an ``LSIS`` identifier with the same number are unrelated.
