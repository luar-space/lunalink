# SUBMISSION.md — Competition Submission Report

## Team Information

| Field | Details |
|-------|---------|
| Team Name | Luar Space |
| Members | Marco Fiscato |
| Institution / Affiliation | Luar Space |
| Contact Email | marco.fiscato@gmail.com |
| Specification | LSIS-AFS (LunaNet Signal-In-Space Recommended Standard — Augmented Forward Signal, LSIS V1.0, 29 January 2025) |

## Language & Technology Stack

| Component | Choice |
|-----------|--------|
| Primary Language | C++20 (signal chain), Python 3.12+ (CLI, tooling, tests) |
| Build System | CMake ≥ 3.15 with Ninja; scikit-build-core for the Python package; [Task](https://taskfile.dev) for every build/test/verify recipe (`task --list` is the inventory) |
| Key Libraries | nanobind (bindings), NumPy, jaxtyping + beartype (runtime shape/type checking). No third-party DSP or FEC library: spreading codes, BCH, LDPC, interleaver, CRC-24Q, BPSK modulation, the FFT and the sync detector are all implemented here |
| Test Framework | Catch2 v3 (C++), pytest (Python), libFuzzer (fuzzing), custom SWIFI harness (fault injection) |
| Platform(s) Tested | macOS ARM64 (primary), Linux x86_64 and arm64, and Windows/MSVC in CI, plus **big-endian s390x** under QEMU to catch byte-order defects |
| Size | ~42 k lines of C++ core (including the generated code tables), 9.4 k lines of C++ tests, 2.4 k lines of Python with 3.6 k lines of Python tests, 16 k lines of documentation source across 53 pages and 3 notebooks; six months of history (190+ commits since 2026-03-02) |

## Architecture Summary

LunaLink implements the complete AFS signal chain in both directions. The
transmit path takes navigation data through frame assembly (subframes SB1–SB4,
CRC-24Q, BCH(51,8), LDPC SF2 (2400,1200) and SF3/SF4 (1740,870), a 60×98 block
interleaver), then spreading (Gold-2046 and Weil-10230 codes, PRN 1–210), BPSK
modulation and I/Q multiplexing into a 12-second, 6000-symbol frame. The
receive path inverts it: frame synchronisation by bipolar cross-correlation,
soft-decision LDPC and exhaustive-ML BCH decoding, CRC verification, and
navigation-message parsing back to typed values, with Time of Transmission
recovered to code phase. Signal-In-Space Error is computed per LSIS §2.2.2.

The C++20 core is organised by subsystem — `codes`, `encoding`, `messages`,
`signal`, `platform`, `core` — each with its own public header tree, and is
exposed to Python through nanobind. The design constraint throughout is flight
software rather than a simulation: no heap allocation after initialisation, no
recursion, fixed-bound loops so worst-case stack depth is computable,
cyclomatic complexity ≤ 10, and single-exit DSP functions so buffer scrubbing
cannot be skipped by an early return. Compiled with `-fno-exceptions
-fno-rtti`. The platform layer carries what a flight build needs around the
signal chain: a Software Configuration Item header (git SHA, LSIS version,
J2000 build timestamp, CRC-32) linked into the binary and verified as step 1
of the power-on self-test, a signal-chain state machine with a legal-transition
matrix, a windowed watchdog, and a telemetry HAL so the chain can be dropped
onto an on-board computer without touching the DSP code.

Beyond the protocol, the implementation carries a mission-assurance case: data
integrity through SEU-resistant enums (non-contiguous, high-Hamming-distance
encodings, so a bit flip yields an illegal rather than a plausible value), TMR
with active repair, mirrored loop variables and CRC-32-self-validating lookup
tables; control-flow integrity through sentinels, a legal-transition matrix and
a windowed watchdog whose *lower* bound catches a radiation-induced jump over a
loop; and containment through three nested Fault Isolation Zones, with the
guarantee that a single radiation event can corrupt at most one 12-second
navigation frame and never the timing system. Each pattern is attributed to its
origin in NASA (JPL/Goddard), ESA (ESTEC) or JAXA practice in
`docs/assurance/mission_assurance.rst`, and each is exercised by fault
injection rather than asserted — `task swifi` drives 42 scenarios across six
techniques against the real injection APIs. A software FMEA
(`docs/assurance/fmea.rst`, nine tables from the code generators to the SISE
calculation) names each failure mode's detection and containment, and
`docs/assurance/fault_coverage.rst` reconciles every FMEA row with the
`// FI-NN` anchor of the test that exercises it (`task swifi-reconcile` fails
on any orphan in either direction).

Those patterns are written in a deliberately restricted dialect, specified in
`docs/assurance/c_plus_plus_guidelines.rst` (*C++20 Style Guide for ECSS Class
B Certifiable Software*). It unions **MISRA C++:2023** — 179 guidelines, 4
Directives and 175 Rules — with the JPL Institutional Coding Standard
(D-60411), Holzmann's Power of Ten, the NASA GSFC C++ standards and the
ESA-commissioned N7 Space C++20 study (N7S-CG-001), **taking the strictest rule
wherever two overlap**, and satisfies the coding-standard requirements of
ECSS-E-ST-40C §5.5.1 and ECSS-Q-ST-80C §5.5. Every clause carries the standards
that impose it inline — the `[[nodiscard]]` mandate is tagged `[MISRA 0.1.2;
JPL Rule 14; P10 Rule 7]` — so a reviewer can trace any restriction to its
source rather than take the dialect on trust. In practice: no exceptions, no
RTTI, no heap after initialisation, `noexcept` on every safety-critical
function, `constinit` for mission data tables, four allowlisted compiler
extensions, and six forbidden optimisation flags that `task check-flags` fails
the build over. clang-tidy runs a MISRA-traceable profile against it, and
`task validate-tidy` proves that profile actually catches the violations it
claims to, against 47 deliberately defective source files.

## Build & Run Instructions

### Prerequisites

- A C++20 compiler (GCC 13+, Clang 16+, or Apple Clang 15+)
- [CMake](https://cmake.org) ≥ 3.15 (Ninja recommended)
- Python ≥ 3.12 with [uv](https://docs.astral.sh/uv/)
- [Task](https://taskfile.dev) (`go-task`)
- [Doxygen](https://www.doxygen.nl), only to build the documentation

On macOS: `brew install cmake ninja uv go-task doxygen`. A `Dockerfile` is
provided for a containerised build. Three commands need network access on
their first run only: `task test-cpp` fetches Catch2, `task interop` clones
LANS-AFS-SIM and `task interop-receiver` clones PocketSDR-AFS.

### Build

```bash
cd lsis-afs          # a clone of this repository
task install         # uv sync --group dev: builds the C++ core + Python package (~1 min)
```

### Run Tests

```bash
task test-cpp        # ~1 min   229 C++ test cases
task test-py         # ~5 s     1242 Python tests (427 skip until `task interop` builds the oracle, 5 until `task interop-receiver` builds the receiver)
task test-acceptance # ~11 min  full slow suite: Monte Carlo Pd, BER, channel, E2E
task swifi           # ~50 s    42 fault-injection scenarios
task interop         # ~10 s    bit-exact cross-check vs an independent implementation
task interop-receiver # ~2 min  L4/L5: a third-party receiver decodes our signal
task performance     # ~3 min   WCET budgets, constant-time gate, BER waterfalls
task compliance-report  # ~3 min  31/31 requirements, 20/20 NFRs, from this run's output
task ci-checks       # ~40 min  every CI workflow, locally and offline
```

### Run Examples

```bash
uv run lunalink generate-codes           # 210 Gold spreading codes -> codes.txt
uv run python examples/01_transmit.py    # 12-second I/Q signal file (~491 MB)
uv run python examples/02_decode.py      # decode a signal file back to navigation data
uv run python examples/03_round_trip.py  # transmit -> decode, bit-exact check
uv run lunalink encode --help            # CLI: frames and I/Q files from FID/TOI/WN/ITOW
uv run lunalink decode --help            # CLI: I/Q file -> FID/TOI/WN/ITOW + CRC flags
```

### Read the Documentation

```bash
task docs-serve      # ~15 s, serves the full documentation at 127.0.0.1:8000
```

Start at `evaluation_guide.html` — it maps every rubric item and every
Delivery Outline deliverable to the document that satisfies it and the
command that verifies it. `api.html` and `api_cpp.html` are the Python and
C++ API references, the latter generated from the headers by Doxygen and
Breathe. `task docs-pdf` produces a single PDF instead. The `docs/` sources
are reStructuredText with Sphinx cross-references, so read them through the
rendered site rather than GitHub's file viewer.

## Gateway Status

| Gateway | Status | Notes |
|---------|--------|-------|
| Gateway 0 — Design & Architecture | **Complete** | `docs/gateway0.rst`; architecture, ICD, tech stack, assurance plan |
| Gateway 1 — Spreading Code Generation | **Complete** | `docs/gateway1.rst`; Gold-2046, Weil-10230, Weil-1500, secondary codes, PRN 1–210, bit-exact vs Annex 3 |
| Gateway 2 — Forward Error Correction | **Complete** | `docs/gateway2.rst`; BCH(51,8) with hard- and soft-decision exhaustive-ML decoders, LDPC SF2/SF3/SF4, 60×98 interleaver, CRC-24Q |
| Gateway 3 — Navigation Message Framing | **Complete** | `docs/gateway3.rst`; SB1–SB4, 12-second frame, all message types |
| Gateway 4 — Baseband Signal Generation | **Complete** | `docs/gateway4.rst`; BPSK modulation, I/Q multiplexing, `LSISIQ` file format (float32 and int16), CSV export |
| Gateway 5 — Frame Sync & Decoding | **Complete** | `docs/gateway5.rst`; bipolar cross-correlation sync detector, full decode chain |
| Gateway 6 — Message Parsing | **Complete** | `docs/gateway6.rst`; typed parsing of all subframe content, Time of Transmission |
| Gateway 7 — Integration & Validation | **Complete** | `docs/gateway7.rst`; end-to-end round trip and cross-implementation validation in **both directions**, L1–L5: LunaLink decodes LANS-AFS-SIM's signal, and PocketSDR-AFS — an independent receiver — decodes LunaLink's |
| Gateway 8 — Documentation & Examples | **Complete** | `docs/gateway8.rst`; full documentation set, generated API references for both languages (434 C++ entities via Doxygen/Breathe, 162 Python symbols via autodoc), examples, tutorial notebooks, performance report |

## Validation Results

All figures below are regenerated by the commands named. The committed
artefacts are in `docs/assurance/evidence/`, so every result can be inspected
as data without running anything.

### Requirements and traceability

| Check | Pass/Fail | Evidence |
|-------|-----------|----------|
| Spec requirements implemented and tested | **PASS** 31/31 | `task compliance-report` → `compliance_report.json` |
| Rubric non-functional requirements | **PASS** 20/20 | same report, `nfr` section |
| Requirement → test traceability | **PASS** 31/31 covered, 0 orphaned tags | `task traceability` → `traceability.json` |
| LSIS V1.0 normative statements dispositioned | 76/76 | `docs/signal/shall_inventory.rst`; conformance view in `docs/signal/pics_proforma.rst` |

### Gateways 1–6 — protocol correctness

| Check | Pass/Fail | Evidence |
|-------|-----------|----------|
| Spreading codes bit-exact vs Annex 3 | **PASS** 210/210 PRNs, every chip | `tests/test_annex3_codes.py`; `uv run lunalink generate-codes` |
| C++ unit and integration suites | **PASS** 229 cases, 2,996,283 assertions | `task test-cpp` |
| Python suite | **PASS** 1242 tests at 100% line coverage | `task test-py`. On a clean checkout 427 of these skip with *"LANS-AFS-SIM not built"* and 5 with *"PocketSDR-AFS not built"* — they are the interop tests, and `task interop` / `task interop-receiver` build what they need, after which all 1242 run |
| Acceptance suite (Monte Carlo Pd, BER, channel, E2E) | **PASS** 264 cases, 3,339,112 assertions, 9 skipped (Release-only timing gates) | `task test-acceptance` |
| Round trip transmit → decode | **PASS** bit-exact | `uv run python examples/03_round_trip.py` |
| Impairments end to end | **PASS** | int16 and int8 round trips, FER sweep at int8, measured int8-vs-float32 implementation loss, burst errors through the interleaver — `test_e2e_impairments.cpp`, `task integration`. Doppler is exercised by the L3 decode of a 7-SV LANS-AFS-SIM signal |

### Gateway 7 — cross-implementation validation

Against **LANS-AFS-SIM** (Takuji Ebinuma's independent C implementation,
BSD-2-Clause), cloned and built by `task interop`:

| Level | Check | Pass/Fail | Evidence |
|-------|-------|-----------|----------|
| L1 | Gold-2046 and Weil-10230 codes, PRN 1–210 | **PASS** | 0 chip mismatches |
| L2 | Frame encoding — BCH, CRC-24, LDPC, interleaver, 5 cases | **PASS** | 0/6000 symbol mismatches |
| L3 | PCPS acquisition, PLL tracking and full decode of a 25 s, 7-SV composite signal with lunar NRHO ephemeris | **PASS** | all 3 subframes CRC-24 verified |
| L4 | PocketSDR-AFS acquires, tracks, LDPC-decodes and CRC-verifies a **LunaLink** signal | **PASS** | AFSD and AFSP acquired at ~40 dB-Hz; `$SB2` logged, which the receiver emits only on a successful LDPC decode and its own CRC-24 check |
| L5 | Parsed-message cross-check: recovered payload vs transmitted | **PASS** | SB2 and CRC-24 identical — `4850D19D…` / `A939B3`. `task interop-receiver`; evidence in `docs/assurance/evidence/interop_l4_receiver.json` |

The two implementations derive spreading codes by structurally different
methods — an 11-bit LFSR with delay table versus tables precomputed from
Appendix C/D — so bit-exact agreement is independent confirmation rather than
two programs reading one table. LANS-AFS-SIM is a transmit-side simulator with
no decoder, so the reverse direction uses PocketSDR-AFS (same authors), the
receiving counterpart: `task interop-receiver` builds it, feeds it a LunaLink
signal, and compares what it recovered.

For other teams: the reference vectors are published separately at
[luar-space/lsis-afs-test-vectors](https://github.com/luar-space/lsis-afs-test-vectors)
with a zero-dependency validator, `scripts/interop/validate_foreign_vectors.py`
checks *any* implementation's vectors against the format, and the `Dockerfile`
ships the `lunalink encode` / `decode` CLI as a workshop image.

### Performance

From the committed `wcet_results.xml` (Release build) and the NFR table in
`docs/gateway8.rst`; `task performance` regenerates them.

| Rubric target | Measured | Margin |
|---------------|----------|--------|
| NFR-1.1 code generation < 1 s per PRN | < 10 ms | > 100× |
| NFR-1.2 frame encoding < 100 ms per frame | LDPC encode SF2 0.79 ms; whole frame < 5 ms | > 20× |
| NFR-1.3 frame decoding < 1 s per frame | LDPC decode SF2 at the forced 50-iteration cap 0.75 ms; `frame_decode` < 20 ms budget | > 50× |
| NFR-1.4 real-time | a 12 s frame is processed in < 1 s | > 12× |
| Sync detection over a 6000-symbol frame | 0.20 ms against a 700 µs budget | 3.5× |
| BCH exhaustive-ML decode | 1.0 µs against a 3 µs budget | 3× |

### Assurance and verification depth

| Check | Pass/Fail | Evidence |
|-------|-----------|----------|
| Fault injection (SWIFI) | **PASS** 42/42 scenarios, 6 techniques | `task swifi` → `swifi_results.json`; FMEA ↔ test reconciliation by `task swifi-reconcile` |
| C++ coverage | Region 93.65%, line 90.27%, function 99.02%, branch 83.56%, MC/DC 40.59%; effective coverage 98.24% once the 123 in-source `NOCOVER(DEFENCE_IN_DEPTH)` paths are excluded | `task coverage-cpp`; `coverage_summary.json`; each exclusion categorised in `docs/assurance/coverage_exclusions.rst` |
| Static analysis | **PASS** | clang-tidy on a MISRA C++:2023-traceable profile, cppcheck, Clang Static Analyzer — `task tidy`, `task cppcheck`, `task clang-sa` |
| Runtime checkers | **PASS** | ASan, UBSan; three libFuzzer harnesses (message parser, frame decoder, FEC) with a committed seed corpus replayed on every pull request and searched nightly — `task sanitize`, `task fuzz` |
| Tool qualification (ECSS-Q-ST-80C §5.5) | **PASS** 47/47 | `task validate-tidy` (47 deliberately defective files, each caught), `task validate-coverage` (llvm-cov proven to report a known gap) |
| WCET budgets | **PASS** | `task wcet`; budgets asserted in `test_wcet.cpp` — table above |
| Timing input-independence | **PASS** | `task constant-time`; the branchless primitives (BPSK, CRC-24, interleaver, BCH ML) timed across adversarial input classes, gated at 14% class-to-class spread |
| Big-endian correctness | **PASS** | s390x under QEMU, `.github/workflows/cross-arch.yml` |
| Committed tree builds and tests | **PASS** | `task validate-export` builds from `git archive`, so nothing depends on an uncommitted file |
| Documentation consistency | **PASS** | `task check-doc-metrics` fails if a coverage figure or the NOCOVER count in the docs drifts from the evidence; `task sync-docstrings` keeps C++ `@param` tags and Python docstrings aligned |

### FEC characterisation

BER/FER waterfalls with 95% Wilson confidence intervals, an error-floor search
finding zero errors in 60,000 frames per point (no floor observable to
6.4 × 10⁻⁵), a convergence knee at ~15 iterations against a shipped cap of 50,
and a quantisation sweep showing int8 costs no measurable performance — so an
8-bit ADC front-end suffices. The shipped hardened decoder lands within 0.15 dB
of an independently written float64 sum-product reference decoder, which
attributes the ~1.0 dB Shannon gap to the code rather than to the
implementation. The BCH layer ships hard- and soft-decision exhaustive-ML
decoders, the soft one worth ~2 dB of link budget. `task ldpc-algo-card`;
`docs/signal/ldpc_algo_card.rst`, `docs/signal/bch_performance.rst`.

## Beyond the rubric

The rubric asks for a reference implementation. This one was built as flight
software with the assurance case a flight programme would need, and that is
where the *Innovation & Extras* category is addressed. Each row is a document
in the rendered site plus the command that regenerates its evidence.

| What | Where | Verify |
|------|-------|--------|
| **Flight-software hardening catalogue** — 13 patterns (SEU-resistant enums, soft-TMR mirroring, parameter TMR with active repair, CFI, saturating invariants, secure scrubbing, memory-section pinning, watchdog feed, self-validating tables, aliasing protection, handoff CRC, ISR safety), a formally-specified state machine, three Fault Isolation Zones — each attributed to NASA, ESA or JAXA practice | `docs/assurance/mission_assurance.rst` | `task swifi` — 42/42 |
| **Software FMEA and fault-coverage report**, reconciled with the tests row by row | `docs/assurance/fmea.rst`, `fault_coverage.rst` | `task swifi-reconcile` |
| **Certifiable C++20 dialect** — MISRA C++:2023 ∪ JPL D-60411 ∪ Power of Ten ∪ NASA GSFC ∪ N7 Space/ESA, strictest rule winning, provenance tagged on every clause | `docs/assurance/c_plus_plus_guidelines.rst` | `task tidy`, `task check-flags` |
| **Tool qualification** per ECSS-Q-ST-80C §5.5 — the analysers proven against deliberately defective code before their verdicts are trusted | `docs/assurance/tool_confidence.rst` | `task validate-tidy`, `task validate-coverage` |
| **Build policy and configuration integrity** — three build tiers (flight, CI digital twin, Python library), forbidden-flag gate, SHA-pinned build manifest, reproducible-build procedure, SCI header verified at POST, `-fstack-usage` evidence | `docs/assurance/build_policy.rst`, `configuration_baseline.rst` | `task check-flags`, `task build-manifest`, `task stack-usage` |
| **MC/DC coverage measured natively** (the DO-178C Level A metric), with every exclusion categorised and milestone-reviewed | `docs/assurance/coverage_evidence.rst`, `coverage_exclusions.rst` | `task coverage-cpp`, `task coverage-effective` |
| **Decoder quality against theory** — shipped decoder vs an independent reference decoder vs the finite-length sphere-packing bound; Shannon gap split into code gap and implementation gap; error floor, convergence, quantisation, codeword symmetry, bit-error forensics | `docs/signal/ldpc_algo_card.rst`, `bch_decoder_design.rst` | `task ldpc-algo-card` |
| **Quantisation and burst-error testing through the full chain**, and the measured int8 implementation loss that sizes the ADC front-end | `docs/signal/validation_plan.rst`, `ldpc_algo_card.rst` | `task integration` |
| **Interoperability in both directions** with two independent implementations, plus tooling for other teams (published vectors, foreign-vector validator, Docker workshop image, CLI) | `docs/interop_results.rst`, `docs/test_vectors.rst` | `task interop`, `task interop-receiver` |
| **Feedback to the standard** — every TBW/TBC/TBD marker in LSIS V1.0 dispositioned; the five interoperability-critical ambiguities written up as SF-001…SF-005 with proposed resolutions and a recommendation to the LNIS working group; proposed bit-level layouts for the 11 message types the spec leaves TBW | `docs/signal/spec_findings_report.rst`, `sisicd.rst`, `shall_inventory.rst` | `task pics-proforma` |
| **Governance** — Software Development Plan, Configuration Management Plan, baseline register, metrics plan with escalation thresholds; the applicable ECSS-E-ST-40C / Q-ST-80C and NASA NPR 7150.2 requirements catalogued, with a compliance matrix against NPR 7150.2 | `docs/governance/`, `docs/standards/` | `task recertify` |
| **Recertification on demand** — every assurance gate re-run against any git ref, emitting a milestone-tagged, signed-off record for a review board | `.github/workflows/recertify.yml` | `task recertify` |
| **Evidence that regenerates itself** — every artefact in `docs/assurance/evidence/` is script-produced, and two gates keep the prose honest: one fails if a documented figure drifts from its artefact, the other builds the *committed* tree rather than the working directory | `docs/evaluation_guide.rst` | `task check-doc-metrics`, `task validate-export` |
| **Documentation as a deliverable** — 53 pages: an evaluation guide, nine per-gateway verification guides, generated API references in both languages (Doxygen/Breathe and autodoc, cross-linked, `sphinx -W` so a dead link fails CI), three executed tutorial notebooks, a PDF build | `task docs-serve` | `task docs-build` |
| **17 CI workflows**, including big-endian s390x, Windows/macOS/Linux matrices, sanitisers, fuzzing, MC/DC coverage, WCET profiling and a `git archive` build — all reproducible offline in one command | `docs/developing.rst` §CI Pipeline | `task ci-checks` |

## PICS Proforma

The full proforma is `docs/signal/pics_proforma.rst`, generated from the
shall-statement inventory by `task pics-proforma` (the generator fails if the
two disagree). Machine-readable copy:
`docs/assurance/evidence/pics_proforma.json`.

| Category | Count |
|----------|-------|
| Items in the proforma | 76 |
| Supported (**Yes**) | 54 |
| Not applicable (**N/A**) | 22 |
| Of which optional (**O**) in the specification | 2 |

Every LSIS V1.0 *shall* is mandatory by definition; the two optional items are
those the specification marks as LNSP implementation flexibility, and both are
supported with the choice documented in the SIS ICD. No item within the
competition's scope is unsupported: all 22 **N/A** entries are either features
requiring RF hardware, which the competition scope excludes (12), or
specification content the spec itself marks To Be Written, leaving nothing to
conform to (10). Three further items carry values the spec marks To Be
Confirmed; these are implemented against documented assumptions recorded in
`docs/signal/spec_findings_report.rst`.

## Stated Limitations

Recorded here rather than left for a reviewer to discover:

- **The reverse-direction demonstration is bounded.** It compares SB2, the
  subframe PocketSDR-AFS logs in decoded form; the signal is generated at
  baseband and quantised rather than captured over the air; and building the
  receiver applies one upstream patch, renaming a file-static `satpos()` that
  collides with RTKLIB's. The patch changes no arithmetic.
- **ECSS Category B conformance is self-assessed**, not certified.
- **Coverage carries waivers.** The overall verdict is `PASS_WITH_WAIVERS`,
  with three files below the 80% per-file review trigger; each waiver is
  categorised and milestone-reviewed in `docs/assurance/coverage_exclusions.rst`.
- **The L3 receiver has documented bounds** — decimation and Q-channel
  aliasing — described in `docs/interop_results.rst`.
- **Sample-clock-offset and phase-noise sweeps are not implemented.** The
  validation plan lists them as future work; what is tested is quantisation
  and burst errors end to end, and Doppler through the L3 cross-decode of a
  foreign signal.
- **The specification's "SNR > 0 dB" is ambiguous**; results are reported under
  both interpretations rather than the more favourable one.
- **The documentation site is not yet published.** `docs-pages.yml` deploys
  it to GitHub Pages once the repository is public; until then `task
  docs-serve` renders the identical build locally in about fifteen seconds.
- **Development history is filtered.** This repository was extracted from a
  larger private tree; the history of the files that have one is present, with
  non-public paths removed and 21 commit messages rewritten where they
  described work outside this repository. Each rewritten message says so. See
  the Provenance section of `README.md`.
