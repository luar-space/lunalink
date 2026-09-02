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
| Build System | CMake ≥ 3.15 with Ninja; scikit-build-core for the Python package; [Task](https://taskfile.dev) for every build/test/verify recipe |
| Key Libraries | nanobind (bindings), NumPy, jaxtyping + beartype (runtime shape/type checking). No third-party DSP or FEC library: spreading codes, BCH, LDPC, interleaver, CRC-24Q, BPSK modulation and the sync detector are all implemented here |
| Test Framework | Catch2 v3 (C++), pytest (Python), libFuzzer (fuzzing), custom SWIFI harness (fault injection) |
| Platform(s) Tested | macOS ARM64 (primary), Linux x86_64 and arm64 in CI, plus **big-endian s390x** under QEMU to catch byte-order defects |

## Architecture Summary

LunaLink implements the complete AFS signal chain in both directions. The
transmit path takes navigation data through frame assembly (subframes SB1–SB4,
CRC-24Q, BCH(51,8), LDPC SF2 (2400,1200) and SF3/SF4 (1740,870), a 60×98 block
interleaver), then spreading (Gold-2046 and Weil-10230 codes, PRN 1–210), BPSK
modulation and I/Q multiplexing into a 12-second, 6000-symbol frame. The
receive path inverts it: frame synchronisation by bipolar cross-correlation,
soft-decision LDPC and exhaustive-ML BCH decoding, CRC verification, and
navigation-message parsing back to typed values. Signal-In-Space Error is
computed per LSIS §2.2.2.

The C++20 core is organised by subsystem — `codes`, `encoding`, `messages`,
`signal`, `platform`, `core` — each with its own public header tree, and is
exposed to Python through nanobind. The design constraint throughout is flight
software rather than a simulation: no heap allocation after initialisation, no
recursion, fixed-bound loops so worst-case stack depth is computable,
cyclomatic complexity ≤ 10, and single-exit DSP functions so buffer scrubbing
cannot be skipped by an early return. Compiled with `-fno-exceptions
-fno-rtti`.

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
techniques against the real injection APIs.

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
claims to, against deliberately defective code.

## Build & Run Instructions

### Prerequisites

- A C++20 compiler (GCC 13+, Clang 16+, or Apple Clang 15+)
- [CMake](https://cmake.org) ≥ 3.15 (Ninja recommended)
- Python ≥ 3.12 with [uv](https://docs.astral.sh/uv/)
- [Task](https://taskfile.dev) (`go-task`)

On macOS: `brew install cmake ninja uv go-task`. A `Dockerfile` is provided for
a containerised build. Two commands need network access on their first run
only: `task test-cpp` fetches Catch2, and `task interop` clones LANS-AFS-SIM.

### Build

```bash
cd lsis-afs          # a clone of this repository
task install         # uv sync --group dev: builds the C++ core + Python package (~1 min)
```

### Run Tests

```bash
task test-cpp        # ~1 min   229 C++ test cases
task test-py         # ~5 s     1237 Python tests (427 skip until `task interop` builds the oracle)
task test-acceptance # ~11 min  full slow suite: Monte Carlo Pd, BER, channel, E2E
task swifi           # ~50 s    42 fault-injection scenarios
task interop         # ~10 s    bit-exact cross-check vs an independent implementation
task performance     # ~3 min   WCET budgets, constant-time gate, BER waterfalls
task ci-checks       # ~40 min  every CI workflow, locally and offline
```

### Run Examples

```bash
uv run lunalink generate-codes           # 210 Gold spreading codes -> codes.txt
uv run python examples/01_transmit.py    # 12-second I/Q signal file (~491 MB)
uv run python examples/03_round_trip.py  # transmit -> decode, bit-exact check
uv run lunalink encode --help            # CLI for frames and I/Q files
```

### Read the Documentation

```bash
task docs-serve      # ~15 s, serves the full documentation at 127.0.0.1:8000
```

Start at `evaluation_guide.html` — it maps every deliverable to the document
that satisfies it and the command that verifies it. `task docs-pdf` produces a
single PDF instead.

## Gateway Status

| Gateway | Status | Notes |
|---------|--------|-------|
| Gateway 0 — Design & Architecture | **Complete** | `docs/gateway0.rst`; architecture, ICD, tech stack, assurance plan |
| Gateway 1 — Spreading Code Generation | **Complete** | `docs/gateway1.rst`; Gold-2046, Weil-10230, Weil-1500, secondary codes, PRN 1–210, bit-exact vs Annex 3 |
| Gateway 2 — Forward Error Correction | **Complete** | `docs/gateway2.rst`; BCH(51,8) with hard- and soft-decision exhaustive-ML decoders, LDPC SF2/SF3/SF4, 60×98 interleaver, CRC-24Q |
| Gateway 3 — Navigation Message Framing | **Complete** | `docs/gateway3.rst`; SB1–SB4, 12-second frame, all message types |
| Gateway 4 — Baseband Signal Generation | **Complete** | `docs/gateway4.rst`; BPSK modulation, I/Q multiplexing, `LSISIQ` file format (float32 and int16) |
| Gateway 5 — Frame Sync & Decoding | **Complete** | `docs/gateway5.rst`; bipolar cross-correlation sync detector, full decode chain |
| Gateway 6 — Message Parsing | **Complete** | `docs/gateway6.rst`; typed parsing of all subframe content |
| Gateway 7 — Integration & Validation | **Complete** | `docs/gateway7.rst`; end-to-end round trip and cross-implementation validation at levels L1–L3. Levels 4–5 (an independent *receiver* decoding LunaLink's signal) are published as vectors, oracles and a validator but **have not been executed** — see Validation Results below |
| Gateway 8 — Documentation & Examples | **Complete** | `docs/gateway8.rst`; full documentation set, examples, tutorial notebooks, performance report |

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
| Spreading codes bit-exact vs Annex 3 | **PASS** | `tests/test_annex3_codes.py`; `uv run lunalink generate-codes` |
| C++ unit and integration suites | **PASS** 229 cases, 2,996,283 assertions | `task test-cpp` |
| Python suite | **PASS** 1237 tests at 100% line coverage | `task test-py`. On a clean checkout 427 of these skip with *"LANS-AFS-SIM not built"* — they are the interop tests, and `task interop` builds the oracle they need, after which all 1237 run |
| Acceptance suite (Monte Carlo Pd, BER, channel, E2E) | **PASS** 264 cases, 3,339,112 assertions, 9 skipped (Release-only timing gates) | `task test-acceptance` |
| Round trip transmit → decode | **PASS** bit-exact | `uv run python examples/03_round_trip.py` |

### Gateway 7 — cross-implementation validation

Against **LANS-AFS-SIM** (Takuji Ebinuma's independent C implementation,
BSD-2-Clause), cloned and built by `task interop`:

| Level | Check | Pass/Fail | Evidence |
|-------|-------|-----------|----------|
| L1 | Gold-2046 and Weil-10230 codes, PRN 1–210 | **PASS** | 0 chip mismatches |
| L2 | Frame encoding — BCH, CRC-24, LDPC, interleaver, 5 cases | **PASS** | 0/6000 symbol mismatches |
| L3 | PCPS acquisition, PLL tracking and full decode of a 25 s, 7-SV composite signal with lunar NRHO ephemeris | **PASS** | all 3 subframes CRC-24 verified |
| L4–L5 | Independent receiver decoding a LunaLink signal; parsed-message cross-checks | **NOT RUN** | Vectors, oracles and a zero-dependency validator are published; no third-party decode is claimed. `docs/interop_results.rst` |

The two implementations derive spreading codes by structurally different
methods — an 11-bit LFSR with delay table versus tables precomputed from
Appendix C/D — so bit-exact agreement is independent confirmation rather than
two programs reading one table. LANS-AFS-SIM is a transmit-side simulator with
no decoder, which is why closing L4–L5 requires a different tool
(PocketSDR-AFS).

### Assurance and verification depth

| Check | Pass/Fail | Evidence |
|-------|-----------|----------|
| Fault injection (SWIFI) | **PASS** 42/42 scenarios, 6 techniques | `task swifi` → `swifi_results.json` |
| C++ coverage | Region 93.65%, line 90.27%, MC/DC 40.59% | `task coverage-cpp`; `coverage_summary.json`. 123 in-source `NOCOVER(DEFENCE_IN_DEPTH)` exclusions, each categorised in `docs/assurance/coverage_exclusions.rst` |
| Static analysis | **PASS** | clang-tidy on a MISRA C++:2023-traceable profile, cppcheck, Clang Static Analyzer — `task tidy`, `task cppcheck`, `task clang-sa` |
| Runtime checkers | **PASS** | ASan, UBSan, libFuzzer with seed corpus — `task sanitize`, `task fuzz` |
| Tool qualification (ECSS-Q-ST-80C §5.5) | **PASS** 47/47 | `task validate-tidy`, `task validate-coverage` |
| WCET budgets | **PASS** | `task wcet`; e.g. sync_detect 64 µs against a 700 µs budget, LDPC SF2 decode 367 µs against 4 ms |
| Timing input-independence | **PASS** | `task constant-time`; class-to-class spread gate on the branchless primitives |
| Big-endian correctness | **PASS** | s390x under QEMU, `.github/workflows/cross-arch.yml` |

### FEC characterisation

BER/FER waterfalls with 95% Wilson confidence intervals, an error-floor search
finding zero errors in 60,000 frames per point (no floor observable to
6.4 × 10⁻⁵), a convergence knee at ~15 iterations against a shipped cap of 50,
and a quantisation sweep showing int8 costs no measurable performance — so an
8-bit ADC front-end suffices. The shipped hardened decoder lands within 0.15 dB
of an independently written float64 sum-product reference decoder, which
attributes the ~1.0 dB Shannon gap to the code rather than to the
implementation. `task ldpc-algo-card`; `docs/signal/ldpc_algo_card.rst`.

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

- **Reverse-direction interoperability (L4–L5) has not been run.** LunaLink
  decodes an independent implementation's signal; the reverse is supported by
  published vectors and a validator but no third-party decode is claimed.
- **ECSS Category B conformance is self-assessed**, not certified.
- **Coverage carries waivers.** The overall verdict is `PASS_WITH_WAIVERS`,
  with three files below the 80% per-file review trigger; each waiver is
  categorised and milestone-reviewed in `docs/assurance/coverage_exclusions.rst`.
- **The L3 receiver has documented bounds** — decimation and Q-channel
  aliasing — described in `docs/interop_results.rst`.
- **The specification's "SNR > 0 dB" is ambiguous**; results are reported under
  both interpretations rather than the more favourable one.
- **Development history is filtered.** This repository was extracted from a
  larger private tree; the history of the files that have one is present, with
  non-public paths removed and 21 commit messages rewritten where they
  described work outside this repository. Each rewritten message says so. See
  the Provenance section of `README.md`.
