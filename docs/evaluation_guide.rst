Evaluation Guide
================

This is the entry point for judging the LunaLink submission to the
**ESA-CCSDS Protocol Development Outreach Competition** (LSIS-AFS track).

It maps the submission to the two authoritative documents a judge scores
against — the *LSIS-AFS Deliverables, Timeline & Gateway Requirements*
(the 100-point rubric, gateways 0–8, NFR-1…NFR-5) and the ESA-CCSDS
*Delivery Outline* (milestone deliverables) — giving for each item the
document that satisfies it and the command that verifies it.

Setup, once
-----------

.. code-block:: bash

   cd lsis-afs      # a clone of this repository
   task install     # uv sync --group dev: builds the C++ core + Python package

Full prerequisites are in :doc:`quickstart`.  Every command on this page is
run from the repository root after ``task install``.

If you would rather not run anything, ``docs/assurance/evidence/`` ships the
generated artefacts from the submission run — ``compliance_report.json``
(per-requirement and per-NFR pass/fail), ``traceability.json``,
``coverage_summary.json``, ``swifi_results.json``, and JUnit XML for the
unit/integration, E2E, WCET, BER and SWIFI suites — so every result on this
page can be inspected as committed data.

----

Minimum Viable Submission (Pass/Fail Gate)
------------------------------------------

All five gate items, one command each.

.. list-table::
   :header-rows: 1
   :widths: 6 24 34 36

   * - #
     - Gate item
     - Command
     - Evidence
   * - 1
     - Generate at least 12 PRN spreading codes correctly
     - ``uv run lunalink generate-codes``
     - **210** Gold codes (not 12) plus Weil primary/tertiary and
       secondary sets in ``test-vectors/codes/``; bit-exact vs Annex 3
       (``tests/test_annex3_codes.py``).  All 12 interim Table 11 codes
       round-trip (:doc:`gateway7`)
   * - 2
     - Encode a complete 12-second navigation frame
     - ``uv run python examples/01_transmit.py``
     - 6000-symbol frame — SP(68) + SB1(52) + interleaved(SB2+SB3+SB4)
       (5880); all subframes encoded
   * - 3
     - Generate I/Q signal files
     - same command
     - ``LSISIQ``-headed file — float32 by default, ``int16`` selectable
       via ``sample_format`` (both TC-2 formats); committed vectors in
       ``test-vectors/signals/``
   * - 4
     - Pass validation tests for implemented features
     - ``task test-cpp && task test-py``
     - 320 C++ test cases + Python suite; 100% Python line coverage gate
   * - 5
     - Include setup/build instructions
     - ``README.md`` + :doc:`quickstart`
     - Plus ``Dockerfile`` for a containerised build

----

Scoring Overview (100 points)
-----------------------------

Where to look for each scored category.

.. list-table::
   :header-rows: 1
   :widths: 22 8 70

   * - Category
     - Points
     - Primary evidence
   * - **Correctness**
     - 40
     - Per-gateway verification guides :doc:`gateway0`–:doc:`gateway8`,
       each mapping deliverable → implementation → verifying command.
       :doc:`signal/compliance_matrix` and :doc:`signal/shall_inventory`
       (76 normative identifiers dispositioned).
       ``task compliance-report`` → **31/31 requirements PASS**.
       Independent corroboration: :doc:`interop_results` — bit-exact
       agreement with LANS-AFS-SIM on codes and frames, and a full
       cross-decode of its signal
   * - **Performance**
     - 20
     - :doc:`gateway8` §*Performance report* — the consolidated NFR
       table, all 20 NFRs.  :doc:`signal/ldpc_performance` and
       :doc:`signal/bch_performance` (BER/FER waterfalls with 95% Wilson
       CIs, error-floor search, convergence knee, quantisation sweep).
       ``task performance``
   * - **Completeness**
     - 20
     - All nine gateways 0–8 delivered, one guide each (table below).
       `Non-Functional Requirements`_: 20/20 PASS.  :doc:`roadmap` states
       what is *not* delivered
   * - **Code Quality**
     - 10
     - :doc:`assurance/c_plus_plus_guidelines` (MISRA C++:2023-traceable
       profile), :doc:`assurance/coverage_evidence` (|cov-region| region,
       |cov-line| line, MC/DC measured natively).  Three static analysers and
       three runtime checkers; ``task lint-check``, ``task tidy``,
       ``task sanitize``
   * - **Innovation & Extras**
     - 10
     - `Beyond the rubric`_ below — flight-software assurance case
       (SWIFI, FMEA, TMR/CFI hardening, tool qualification, big-endian
       CI, reference-decoder gap analysis), none of it required by the
       rubric

----

Gateway deliverables (0–8)
--------------------------

The rubric defines nine gateways.  Each has one verification guide stating
what it delivers, its validation checklist, and the exact commands that
verify each item.

.. list-table::
   :header-rows: 1
   :widths: 6 26 40 28

   * - GW
     - Deliverable
     - Success metric (rubric)
     - Guide
   * - 0
     - Design & Architecture
     - Clear architecture with defined components; dev environment ready;
       testing framework established
     - :doc:`gateway0`
   * - 1
     - Spreading code generation
     - All codes match Annex 3 hex; < 1 s per PRN; all 12 interim
       Table 11 codes working
     - :doc:`gateway1`
   * - 2
     - Forward error correction
     - Encode < 100 ms/frame; decode < 1 s/frame;
       BER < 10\ :sup:`-5` at SNR > 0 dB
     - :doc:`gateway2`
   * - 3
     - Navigation message framing
     - Valid frames, all subframes; 68 + 52 + 5880 = 6000 symbols;
       12-second frame timing
     - :doc:`gateway3`
   * - 4
     - Baseband signal generation
     - 12-second I/Q files; correct chip and symbol rates
     - :doc:`gateway4`
   * - 5
     - Frame synchronization & decoding
     - Decode < 1 s; frame sync > 99% at SNR > 0 dB
     - :doc:`gateway5`
   * - 6
     - Message parsing
     - All subframe types parse; ToT accurate; all message types handled
     - :doc:`gateway6`
   * - 7
     - Integration & validation
     - Round-trip 100% accuracy; all compliance checks pass; successful
       interoperability testing
     - :doc:`gateway7`
   * - 8
     - Documentation & examples
     - Anyone can build and run; all features documented; reproducible
       results
     - :doc:`gateway8`

.. _non-functional requirements:

Non-Functional Requirements
---------------------------

All 20 rubric NFRs are tracked by ``scripts/compliance_report.py`` and
regenerated by ``task compliance-report`` into
``docs/assurance/evidence/compliance_report.json``.  Each is evaluated
against a JUnit result, a benchmark artefact, or a concrete check of the
delivered repository — none is asserted.

.. list-table::
   :header-rows: 1
   :widths: 12 34 30 24

   * - ID
     - Requirement (target)
     - Result
     - How it is checked
   * - NFR-1.1
     - Code generation < 1 s per PRN
     - PASS — < 10 ms
     - ``task wcet`` benchmark
   * - NFR-1.2
     - Frame encoding < 100 ms per frame
     - PASS — < 5 ms
     - ``task wcet`` benchmark
   * - NFR-1.3
     - Frame decoding < 1 s per frame
     - PASS
     - E2E latency test
   * - NFR-1.4
     - Real-time: faster than signal duration
     - PASS — 12 s signal in < 1 s
     - ``task wcet`` benchmark
   * - NFR-2.1
     - Spreading code accuracy: 100% match with Annex 3
     - PASS — 210/210 PRNs, every chip
     - ``[LSIS-AFS-101]`` tests
   * - NFR-2.2
     - BER < 10\ :sup:`-5` at SNR > 0 dB
     - PASS (both SNR interpretations)
     - ``task ber`` waterfalls
   * - NFR-2.3
     - Frame sync reliability > 99% at SNR > 0 dB
     - PASS
     - ``[LSIS-AFS-710]`` tests
   * - NFR-2.4
     - Time reconstruction accurate to code phase
     - PASS
     - ``[LSIS-AFS-810]`` tests (18)
   * - NFR-3.1
     - Unit tests for all major components
     - PASS — 320 test cases
     - JUnit totals
   * - NFR-3.2
     - Integration tests: end-to-end flow
     - PASS
     - E2E suite
   * - NFR-3.3
     - Test coverage > 90%
     - PASS — |cov-region| region
     - ``coverage_summary.json``
   * - NFR-3.4
     - Test vectors reproducible
     - PASS
     - Exporters + committed vector tree
   * - NFR-4.1
     - Clear API documentation: all public APIs documented
     - PASS
     - :doc:`api` + ``sync_docstrings.py`` executed as the check
   * - NFR-4.2
     - Working examples for all features
     - PASS
     - 3 examples + 3 notebooks present
   * - NFR-4.3
     - Error messages give actionable guidance
     - PASS
     - Typed error enumeration (:doc:`signal/icd`)
   * - NFR-4.4
     - Runtime configuration support
     - PASS
     - ``ldpc_iterations``, ``sample_rate``, ``prn_id``,
       ``raise_on_error`` verified as real parameters
   * - NFR-5.1
     - Modular architecture, clean component boundaries
     - PASS
     - :doc:`signal/architecture` + module tree
   * - NFR-5.2
     - Separation of concerns: independent modules
     - PASS
     - Public-header tree per subsystem
   * - NFR-5.3
     - Consistent coding style enforced via tooling
     - PASS
     - clang-format, clang-tidy, cppcheck, ruff — all CI-gated
   * - NFR-5.4
     - Version control with meaningful history
     - PASS
     - Commit count, span and mean subject length measured against stated
       thresholds by ``scripts/compliance_report.py``; the figures for this
       run are in ``compliance_report.json``.  See the Provenance section of
       ``README.md`` for how this history relates to the tree it was
       extracted from

.. code-block:: bash

   task compliance-report
   # 31/31 requirements PASS; NFRs: 20 PASS

One command, ~3 minutes from a clean clone.  It runs the unit, integration,
SWIFI, WCET and BER suites and grades against their JUnit output — NFR-1.1,
NFR-1.2 and NFR-1.4 come from ``wcet_results.xml``, NFR-2.2 from
``ber_results.xml``.  The report will not grade an NFR from evidence the run
did not produce, and does not fall back to the committed evidence, so a
regression shows up as a failure rather than as last week's pass.  The
committed ``docs/assurance/evidence/compliance_report.json`` is the
submission run.

----

Technical constraints
---------------------

.. list-table::
   :header-rows: 1
   :widths: 12 40 48

   * - ID
     - Constraint
     - Status
   * - TC-1
     - Open-source language and libraries; standard library or
       equivalent for core algorithms
     - Met.  C++20 core with no third-party DSP dependency; runtime
       Python deps are NumPy plus two typing libraries.  No proprietary
       libraries.  See :doc:`tech_stack`
   * - TC-2
     - Data formats: binary I/Q (int16, float32), CSV, JSON
     - Met.  The ``LSISIQ`` container carries **both** int16 and float32,
       declared in its header and selected with
       ``write_iq_file(..., sample_format=...)``;
       :func:`~lunalink.afs.read_iq_file` reads either back
       (:doc:`test_vectors`).  CSV via ``write_iq_csv``; JSON for parsed
       vectors and every evidence artefact
   * - TC-3
     - Annex 1 LDPC matrices, Annex 3 reference codes, LSIS spec
     - Met.  All three under ``docs/references/``; matrices converted by
       ``scripts/gen_ldpc_tables.py``, codes validated by
       ``tests/test_annex3_codes.py``
   * - TC-4
     - Cross-platform (Windows, Linux, macOS); no RF hardware; standalone
     - Met with a stated scope: Python suite runs on
       ``windows-latest``, ``macos-14`` and ``ubuntu-latest``; the C++
       suite runs on Linux x86-64, arm64 and **big-endian s390x**.  No RF
       dependency; offline after install

----

Delivery Outline conformance
----------------------------

The ESA-CCSDS Delivery Outline names its deliverables per milestone.  Each
is listed here with the document that satisfies it.

Milestone 1 — Standards Compliance & Architecture (§3.2.1)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 26 32 42

   * - Outline deliverable
     - Where it is
     - How to verify
   * - LSIS-AFS compliance matrix
       (mandatory vs optional elements)
     - :doc:`signal/compliance_matrix` — every requirement by gateway;
       :doc:`signal/shall_inventory` — all 76 normative identifiers in
       LSIS V1.0 with disposition (49 implemented, 12 RF/out-of-scope,
       10 TBW, 3 TBC, 2 FLEX)
     - ``task compliance-report`` → 31/31 requirements PASS
   * - Signal chain block diagram
     - :doc:`signal/signal_chain` — full TX block diagram (GD1.1 → GD4.2)
       with rates, types and per-stage data flow
     - ``uv run python demo/v1_plot.py`` renders the chain;
       :doc:`notebooks/01_spreading_codes` walks it stage by stage
   * - Implementation architecture description
     - :doc:`gateway0` is the Gateway 0 deliverable document;
       :doc:`signal/architecture` records the design decisions and the
       module → gateway map
     - ``task docs-pdf`` builds ``gateway0`` as a standalone PDF
   * - Interface and configuration definition
     - :doc:`signal/icd` — Interface Control Document: complete C++ API,
       Python bindings, HAL interfaces, wire formats, error enumeration.
       :doc:`signal/sisicd` — bit-level payload layouts for the message
       types LSIS V1.0 leaves TBW.
       :doc:`assurance/configuration_baseline` — build configuration and
       release baseline
     - ``task test-cpp`` exercises every interface in the ICD;
       ``task build-manifest`` emits the configuration record
   * - Clear traceability to the CCSDS specification
     - :doc:`assurance/traceability_matrix`; canonical machine-readable
       matrix at ``docs/assurance/evidence/traceability.json``
     - ``task traceability`` → 31 requirements, 100% covered,
       **0 orphaned test tags** (CI-gated by ``traceability.yml``)

Milestone 2 — Reference Implementation (§3.2.2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 26 32 42

   * - Outline deliverable
     - Where it is
     - How to verify
   * - Software implementation of LSIS-AFS signal framing
     - :doc:`gateway3` (framing), :doc:`gateway5` (sync + decode),
       :doc:`gateway6` (message parsing).  Source: ``cpp/messages/``
     - ``uv run python examples/01_transmit.py`` — 6000-symbol,
       12-second frame; ``task test-cpp``
   * - Modulation and coding per specification
     - :doc:`gateway1` (spreading codes), :doc:`gateway2` (BCH + LDPC +
       CRC-24 + interleaver), :doc:`gateway4` (BPSK, I/Q multiplexing).
       Algorithm records: :doc:`signal/algorithms`,
       :doc:`signal/bch_decoder_design`, :doc:`signal/ldpc_algo_card`
     - ``uv run lunalink generate-codes`` — 210 PRN codes, bit-exact vs
       Annex 3 (``tests/test_annex3_codes.py``); ``task test-cpp``
   * - Configuration documentation
     - :doc:`assurance/configuration_baseline` (baseline + reproducible
       build procedure), :doc:`assurance/build_policy` (flight vs
       non-flight build configurations), :doc:`tech_stack` (toolchain and
       dependency pins), :doc:`governance/baseline_register`
     - ``task build-manifest`` → SHA-pinned build manifest;
       ``task check-flags`` asserts the compiled flag set
   * - Test vectors and validation methodology
     - :doc:`signal/validation_plan` — the five-level validation hierarchy
       and edge-case register.  :doc:`test_vectors` — the binary
       interchange format.  Vectors themselves in ``test-vectors/``
     - ``uv run python tests/generate_test_vectors.py`` regenerates all
       five levels; ``task test-py``

Milestone 3 — Validation and Interoperability Demonstration (§3.2.3)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 26 32 42

   * - Outline deliverable
     - Where it is
     - How to verify
   * - Bit error rate (BER) performance
     - :doc:`signal/ldpc_performance` — SF2 and SF3/SF4 waterfalls with
       95% Wilson confidence intervals, Shannon-gap decomposition, error
       floor search (zero errors in 60,000 frames/point).
       :doc:`signal/bch_performance` — hard- vs soft-decision FER
       waterfalls (~2 dB soft gain)
     - ``task ber`` (BER/FER sweeps); ``task performance`` (full run)
   * - Compliance with signal structure requirements
     - :doc:`signal/compliance_matrix`, :doc:`signal/shall_inventory`,
       and the per-gateway guides :doc:`gateway1`–:doc:`gateway7`
     - ``task compliance-report`` → ``compliance_report.json``
   * - Parameter sensitivity testing
     - :doc:`signal/validation_plan` §*Signal Impairment & Robustness* —
       Doppler/CFO sweep (−10…+10 kHz), sample-clock offset (0–50 ppm),
       oscillator phase noise (Wiener, 0–1e-3 rad²/sample), and I/Q
       quantisation (float32 / int16 / int8).
       :doc:`signal/ldpc_performance` §*Convergence Speed* (iteration
       sweep, knee at ~15 against a cap of 50) and §*Quantization
       Resilience* (input-LLR sweep; int8 costs no measurable
       performance)
     - ``task integration`` runs ``test_channel_impairments.cpp`` and
       ``test_e2e_impairments.cpp``; ``task ber`` reruns the sweeps
   * - Reproducibility of results
     - :doc:`assurance/configuration_baseline` — bit-for-bit reproducible
       build procedure.  All stochastic runs use fixed PRNG seeds
       (documented per page).  Every artefact on this site is script-
       generated, never hand-maintained
     - ``task ci-checks`` reproduces all 16 CI workflows locally;
       ``task recertify`` re-runs every gate against any git ref and
       emits a signed-off record
   * - **Cross-decoding of independently generated signals**
     - :doc:`interop_results` — L1 codes and L2 frames bit-exact against
       LANS-AFS-SIM (Ebinuma, independent C implementation); L3 acquires,
       tracks and fully decodes a 25 s, 7-SV LANS-AFS-SIM signal with all
       three subframes CRC-24 verified
     - ``task interop`` — clones and builds LANS-AFS-SIM, then runs
       L1–L3.  Pass criterion for codes and frames is bit-exact
       equality, not statistical agreement
   * - Independent verification of standards compliance
     - :doc:`interop_results` (agreement with an implementation that
       derives codes by a structurally different method);
       :doc:`assurance/tool_confidence` (analysers qualified against
       deliberately defective code per ECSS-Q-ST-80C §5.5)
     - ``task validate-tidy``, ``task validate-coverage``
   * - Documented interoperability outcomes
     - :doc:`interop_results` — full methodology, per-level results,
       the interoperability report template instance, and stated
       limitations
     - ``task interop``; vectors for other teams published at
       `luar-space/lsis-afs-test-vectors
       <https://github.com/luar-space/lsis-afs-test-vectors>`__

----

Final Submission Package
------------------------

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Required
     - Delivered
   * - Complete source code
     - ``cpp/`` (C++20 core), ``src/lunalink/`` (Python package),
       ``tests/``, ``scripts/``.  Layout described in
       :doc:`signal/architecture` and :doc:`developing`
   * - Build/setup instructions
     - ``README.md`` and :doc:`quickstart`; ``Dockerfile`` for a
       containerised build
   * - Test suite with results
     - ``task test-cpp && task test-py``; committed JUnit XML and
       summaries in ``docs/assurance/evidence/``
   * - Documentation (setup, usage, API)
     - This site — :doc:`quickstart`, :doc:`api`, :doc:`signal/icd`
   * - Examples demonstrating functionality
     - ``examples/`` (transmit, decode, round-trip), ``demo/``, and the
       three tutorial notebooks (:doc:`gateway8`)
   * - Validation report showing compliance
     - :doc:`gateway7` (GD7.5) and ``compliance_report.json``;
       performance analysis in :doc:`gateway8`

Optional-but-recommended items — performance analysis, design
documentation, additional test vectors, visualization tools, BER
performance curves — are all delivered; see :doc:`gateway8` and
:doc:`signal/ldpc_performance`.

----

.. _beyond the rubric:

Beyond the rubric
-----------------

The deliverables above are what the competition asks for.  Beyond them the
submission carries a flight-software assurance case the rubric does not
require, which is where the *Innovation & Extras* category is addressed.
Each claim has a verifying command in the *What makes this implementation
different* section of ``README.md``:

* **Defence in depth** — :doc:`assurance/mission_assurance`: NASA/ESA/JAXA
  hardening patterns (SEU-resistant enums, TMR with active repair, CFI
  sentinels, windowed watchdog, Fault Isolation Zones), each attributed to
  its origin in flight practice.  Proven by ``task swifi`` — 42/42
  fault-injection scenarios across 6 techniques.
* **Fault analysis** — :doc:`assurance/fmea`, :doc:`assurance/fault_coverage`.
* **Verification depth** — :doc:`assurance/coverage_evidence` (line, branch
  and MC/DC, the DO-178C Level A structural metric),
  :doc:`assurance/coverage_exclusions`, :doc:`assurance/metrics_plan`.
* **Tool qualification** — :doc:`assurance/tool_confidence`: analysers made
  to prove themselves against deliberately defective code, per
  ECSS-Q-ST-80C §5.5.
* **Decoder quality against theory** — :doc:`signal/ldpc_algo_card`: the
  shipped decoder measured against an independent reference decoder *and*
  the finite-length sphere-packing bound, splitting the Shannon gap into
  code gap and implementation gap.
* **Standards mapping** — :doc:`standards/esa/ecss_standards`,
  :doc:`standards/nasa/npr7150_standards`,
  :doc:`standards/nasa_compliance_addendum`.
* **Governance** — :doc:`governance/sdp`, :doc:`governance/scmp`,
  :doc:`governance/baseline_register`.
* **Spec feedback** — :doc:`signal/spec_findings_report`: ambiguities found
  in LSIS V1.0 with proposed formal resolutions, which the Delivery Outline
  names as one of the competition's objectives.

----

.. _stated limits:

Stated limits
-------------

Collected here so they are not left for a reader to discover:

* **Reverse-direction interoperability (L4/L5) is not demonstrated.**
  LunaLink decodes an independent implementation's signal
  (:doc:`interop_results` L3).  The reverse — an independent receiver
  decoding a LunaLink signal — is supported by published vectors, oracles
  and a validator, but **has not been run**.
* **Development history is filtered, not complete.**  LunaLink was extracted
  from a larger private tree.  The history here covers the files that have
  one, with non-public paths removed and 21 commit messages rewritten where
  they described work outside this repository; each rewritten message says
  so.  Files authored after the extraction — the assurance documents and
  generated evidence — start at the extraction commit.  See the Provenance
  section of ``README.md``.
* **Coverage waivers.**  C++ line coverage is |cov-line| with three files
  below the 80% per-file review trigger; the overall verdict is
  ``PASS_WITH_WAIVERS``, each waiver categorised in
  :doc:`assurance/coverage_exclusions`.  123 in-source
  ``NOCOVER(DEFENCE_IN_DEPTH)`` annotations mark paths that execute only
  under radiation-induced bit flips.
* **L3 receiver scope.**  The demodulation chain runs at 1 sample/chip,
  which aliases the AFS-Q pilot onto the I channel; sufficient for the
  cross-decode, not a flight-credible receiver design
  (:doc:`interop_results` §*Known limitations*).
* **Spec ambiguity.**  "SNR > 0 dB" is ambiguous; both interpretations are
  reported and the requirement is met under each
  (:doc:`signal/ldpc_performance`).
* **ECSS Category B conformance is self-assessed, not certified.**

----

A note on terminology
---------------------

A **gateway** is one of the nine incremental deliverables defined by the
competition rubric, Gateway 0 (Design & Architecture) through Gateway 8
(Documentation & Examples).  Gateway 0 also serves as the Milestone-1
deliverable document for the ESA-CCSDS Delivery Outline.

Three requirement-ID families appear across the project — compliance-matrix
row IDs (``CM-nnn``), specification shall IDs (``LSIS-nnn``), and
implementation tags (``[LSIS-AFS-nnn]``).  :doc:`signal/compliance_matrix`
opens with a note explaining how they relate; they are not
interchangeable.
