# LunaLink — LSIS-AFS Reference Implementation

Open-source reference implementation of the **LunaNet Signal-In-Space
Augmented Forward Signal (LSIS-AFS)** protocol, as specified in LSIS V1.0
(29 January 2025).

Built as part of the **ESA-CCSDS Protocol Development Outreach Competition**.

> **Judging this submission? Start here.** After `task install`, run
> `task docs-serve` (~15 s) and open
> <http://127.0.0.1:8000/evaluation_guide.html> — the Evaluation Guide maps
> every Delivery Outline deliverable to the document that satisfies it and the
> command that verifies it.
>
> The `docs/` sources are reStructuredText: read them through that local site
> rather than GitHub's file viewer, which does not resolve Sphinx
> cross-references. `task docs-pdf` produces a single PDF instead, and
> `docs/evaluation_guide.rst` is legible as plain text at a pinch.

## Features

- Complete AFS signal chain: spreading codes, FEC (BCH + LDPC), framing,
  BPSK modulation, IQ multiplexing — and the inverse decode chain back to
  parsed navigation data
- Frame sync detector with bipolar cross-correlation
- Signal-In-Space Error (SISE) computation per LSIS §2.2.2
- C++20 core with Python bindings (nanobind)

## What makes this implementation different

If you have fifteen minutes, this is the tour — `task install`, then three
commands that exercise the claims we would most want checked:

```bash
task interop      # ~10 s  bit-exact against an independent implementation
task swifi        # ~50 s  42/42 fault-injection scenarios
task test-cpp && task test-py   # ~1 min  229 C++ cases, 1237 Python tests
```

The nine sections below expand on that. Every claim in them has a command
beside it or in
[Verifying the implementation](#verifying-the-implementation). Nothing here
is asserted.

### 1. Defence in depth — a layered architecture, not a hardening checklist

The assurance case is written down, not implied. Two documents carry it:
[mission_assurance.rst](docs/assurance/mission_assurance.rst) (the pattern
catalogue, each pattern attributed to its origin in NASA (JPL/Goddard), ESA
(ESTEC) or JAXA flight practice) and
[c_plus_plus_guidelines.rst](docs/assurance/c_plus_plus_guidelines.rst) (the
dialect those patterns are written in).

- **Dialect** — flight code is a restricted C++20 that unions MISRA C++:2023
  (179 guidelines: 4 Directives + 175 Rules), the JPL Institutional Coding
  Standard (D-60411), Holzmann's Power of Ten, the NASA GSFC C++ standards and
  the ESA-commissioned N7 Space C++20 study (N7S-CG-001), **taking the
  strictest rule wherever two overlap**, under ECSS-E-ST-40C §5.5.1 and
  ECSS-Q-ST-80C §5.5. Every rule carries its provenance inline — the
  `[[nodiscard]]` mandate reads `[MISRA 0.1.2; JPL Rule 14; P10 Rule 7]` — so
  a reviewer can trace any restriction back to the standard that imposes it.
  No exceptions, no RTTI, no heap after init, four allowlisted compiler
  extensions, and six *forbidden* optimisation flags (`-ffast-math`,
  `-fstrict-aliasing` and `-fomit-frame-pointer` among them) that
  `task check-flags` fails the build over
- **Construction** — zero heap after init, no recursion, fixed-bound loops
  (enabling formal worst-case stack depth), cyclomatic complexity ≤ 10, and
  single-exit DSP functions so buffer scrubbing can never be skipped by an
  early `return`
- **Data integrity** — SEU-resistant enums using non-contiguous,
  high-Hamming-distance patterns, so a bit flip yields an *illegal* value
  rather than a plausible one; TMR with active repair, since passive voting
  is insufficient over a multi-year mission; mirrored loop variables to
  catch ALU corruption mid-calculation; `CheckedRange<T,Min,Max>` saturating
  invariants; CRC32-self-validating lookup tables; memory-section pinning
- **Control-flow integrity** — CFI sentinels, legal-transition matrices,
  forbidden-state detection, proof-of-progress ticks, and a **windowed**
  watchdog whose *lower* bound catches a radiation-induced jump over a loop
- **Configuration integrity** — an SCI header (git SHA, LSIS version, J2000
  build timestamp, CRC-32) embedded in the binary at link time and verified
  as POST step 1: nothing transmits a symbol until the build verifies itself
- **Containment** — three nested Fault Isolation Zones, annotated in-source
  as `// FIZ-1/2/3`, bounding the blast radius of any detected fault. The
  guarantee: **a single radiation event can corrupt at most one 12-second
  navigation frame — never the timing system, never the OBC.** No symbol
  stream reaches the SDR until the complete frame passes a post-generation
  checksum
- **Proof** — `task swifi` runs 42/42 fault-injection scenarios across 6
  techniques, driving real injection APIs, all `#ifdef`-gated out of flight
  builds

C++ line coverage is 90.27% by design, not omission: **123 in-source
`NOCOVER(DEFENCE_IN_DEPTH)` annotations** mark paths that execute only when
a cosmic ray flips a bit. Those paths are covered by SWIFI at the pattern
level, and every exclusion is categorised and milestone-reviewed in
[coverage_exclusions.rst](docs/assurance/coverage_exclusions.rst).

### 2. Cross-implementation validation, in both directions

Not a diff against stored vectors — LunaLink **acquires and decodes another
implementation's signal**. Against LANS-AFS-SIM (Ebinuma's independent C
implementation), via `task interop`:

| Level | Check | Result |
|:--|:--|:--|
| L1 | Gold-2046 + Weil-10230 codes, PRN 1–210 | 0 chip mismatches |
| L2 | Frame encoding, 5 cases (BCH, CRC-24, LDPC, interleaver) | 0/6000 symbol mismatches |
| L3 | PCPS acquisition + PLL tracking + full decode of a 25 s, 7-SV composite signal with lunar NRHO ephemeris | all 3 subframes CRC-24 verified |

The two implementations derive spreading codes by **structurally different
methods** — an 11-bit LFSR with delay table versus tables precomputed from
Appendix C/D — so bit-exact agreement is independent confirmation, not two
programs reading one table.

Levels 4–5 are the reverse direction — an independent receiver decoding
*LunaLink's* signals, and parsed-message cross-checks. Vectors and bundled
oracles for both are published in
[lsis-afs-test-vectors](https://github.com/luar-space/lsis-afs-test-vectors)
with a zero-dependency validator, targeting PocketSDR-AFS. **A completed
third-party decode of a LunaLink signal is not claimed here** — the
demonstrated cross-decode is L3, in the direction above. See
[interop_results.rst](docs/interop_results.rst) for exactly what has and has
not been run.

### 3. Decoder quality measured against theory, not just "it works"

An independently written, unhardened float64 sum-product reference decoder
exists purely as the theoretical ceiling. Comparing the shipped decoder
against it *and* against the finite-length sphere-packing bound splits the
Shannon gap into two attributable halves — **code gap** (inherent to the
n=2400 design) and **implementation gap** (hardening, clamping, schedule).
The shipped, TMR/CFI-hardened decoder lands **within 0.15 dB of reference
belief propagation**: the ~1.0 dB total gap is the code's, not ours.
(`task ldpc-algo-card` regenerates the comparison.)

### 4. FEC characterised, not merely tested

BER/FER waterfalls with 95% Wilson confidence intervals; an error-floor
search finding **zero errors in 60,000 frames per point** (no floor
observable to 6.4 × 10⁻⁵); a convergence knee at ~15 iterations against a
shipped cap of 50; and a quantisation sweep showing **int8 costs no
measurable performance**, so an 8-bit ADC front-end suffices — a
receiver-design conclusion, not just a test result. The BCH layer ships two
exhaustive-ML decoders, hard- and soft-decision, the soft one worth ~2 dB of
link budget. `task ldpc-algo-card` regenerates the LDPC characterisation
(waterfalls, error floor, convergence, quantisation); `task performance`
runs the BCH and channel simulations.

### 5. Verification depth

Three static analysers (clang-tidy on a MISRA C++:2023-traceable profile
whose per-rule compliance record is
[c_plus_plus_guidelines.rst](docs/assurance/c_plus_plus_guidelines.rst),
cppcheck, Clang Static Analyzer) and three runtime checkers (ASan, UBSan,
libFuzzer with seed corpus and nightly deep-fuzz). MC/DC measured natively —
the DO-178C Level A structural metric. CI runs on **big-endian s390x**
alongside arm64, catching byte-order defects x86/ARM CI never will.

Determinism is measured, not claimed — and scoped honestly. The
input-independent primitives (BPSK modulator, CRC-24, interleaver, and the
exhaustive-ML BCH decoder) are timed across adversarial input classes
(all-zeros, all-ones, pseudo-random); `task constant-time` fails the build if
class-to-class spread exceeds 14%, a ceiling set at 2× the worst value
measured over 8 runs. The LDPC and frame decoders are deliberately *excluded*:
they early-terminate on a satisfied syndrome, so an all-zeros payload
genuinely decodes faster, and forbidding that would forbid a correct
optimisation. Their worst case is bounded instead by forced full-iteration
budgets (`LDPC decode SF2 (50 iter) < 4 ms`), and those measured bounds set
the windowed watchdog's thresholds.

### 6. Tools qualified, not trusted

Most projects assert their analysers work. These are made to prove it
against deliberately defective code, per ECSS-Q-ST-80C §5.5:

```bash
task validate-tidy      # proves clang-tidy catches the MISRA violations claimed
task validate-coverage  # proves llvm-cov correctly reports known gaps
```

### 7. Evidence that regenerates itself

31/31 requirements passing with **0 orphaned test tags**, 20/20 NFRs
passing, 76 spec
`shall` statements dispositioned, and every artefact — compliance report,
traceability matrix, coverage evidence, SWIFI report, stack usage, build
manifest, algorithm cards — produced by script rather than maintained by
hand. An on-demand recertification workflow runs every gate against any git
ref and emits a signed-off, milestone-tagged record. `task ci-checks`
reproduces the CI gate locally and offline — build, lint, formatting, Clang
Static Analyzer, both test suites, SWIFI, traceability, doc metrics,
clang-tidy and its qualification, sanitisers, docs. The workflows outside it
are the long ones, each its own task: `task performance` (WCET, BER),
`task test-acceptance`, `task coverage-cpp`, `task fuzz`, `task cross-arch`.

### 8. Documentation organised around assessment

Eight per-gateway verification guides map each deliverable to its
implementation and to the exact command that verifies it, rather than
leaving reviewers to derive that from an architecture diagram. Start at
[docs/evaluation_guide.rst](docs/evaluation_guide.rst).

### 9. Stated limits

Where something is bounded, it says so: the L3 receiver's decimation and
Q-channel aliasing, the spec's ambiguous "SNR > 0 dB" reported under both
interpretations, ECSS Category B as self-assessed rather than certified, and
every coverage exclusion justified in a reviewed register. The positive
claims above are meant to be read in that light.

## Prerequisites

Building from source needs:

- A C++20 compiler (GCC 13+, Clang 16+, or Apple Clang 15+)
- [CMake](https://cmake.org) ≥ 3.15 (Ninja recommended)
- Python ≥ 3.12 with [uv](https://docs.astral.sh/uv/)
- [Task](https://taskfile.dev) (`go-task`) — runs every build/test/verify recipe

On macOS: `brew install cmake ninja uv go-task`. A `Dockerfile` is provided
for a containerised build if you prefer not to install toolchains.

## Quick Start

```bash
cd lsis-afs         # a clone of this repository
task install        # uv sync --group dev: builds the C++ core + Python package
```

Then generate the competition artefacts directly from the CLI:

```bash
uv run lunalink generate-codes           # all 210 Gold spreading codes (hex)
uv run lunalink encode --help            # encode a 12-s frame or I/Q signal file
uv run python examples/01_transmit.py    # 12-s I/Q signal file (writes ~491 MB)
uv run python examples/03_round_trip.py  # transmit → decode, bit-exact check
```

Library-only use (no dev tools) also works with plain pip: `pip install .`

## For evaluators

**Nothing to run?** `docs/assurance/evidence/` ships the generated artefacts
from the submission run as committed data — `compliance_report.json`
(per-requirement and per-NFR pass/fail), `traceability.json`,
`coverage_summary.json`, `swifi_results.json`, and JUnit XML for the
unit/integration, E2E, WCET, BER and SWIFI suites. Every result claimed here
can be inspected without a toolchain.

Each pass/fail gate item is one command, run from the repo root after
`task install` (~1 min):

| Gate item | Command | Time | Evidence |
|:--|:--|:--|:--|
| ≥ 12 PRN spreading codes | `uv run lunalink generate-codes` | < 1 s | 210 Gold codes written to `codes.txt` as 210 × 512 hex, bit-exact vs Annex 3 (`tests/test_annex3_codes.py`); full Gold + Weil + secondary sets in `test-vectors/codes/` |
| Complete 12-second frame | `uv run python examples/01_transmit.py` | ~1 s | 6000-symbol frame, all subframes |
| I/Q signal files | same command | — | `LSISIQ`-headed file — float32 or int16 (`sample_format=`), read back with `read_iq_file`; short vectors in `test-vectors/signals/` (writes a 491 MB `signal.iq`) |
| Validation tests | `task test-cpp && task test-py` | ~1 min | full unit/integration suites — 229 C++ cases, 1237 Python tests |
| Setup/build instructions | this file + [docs/quickstart.rst](docs/quickstart.rst) | — | — |

Timings are wall-clock from a clean clone on an Apple M-series laptop, and
include the first cold C++ build. Two commands need network on their first
run only: `task test-cpp` (fetches Catch2) and `task interop` (clones
LANS-AFS-SIM); everything else is offline.

`task ci-checks` runs the whole CI gate in one command. It excludes the long
workflows — `task performance`, `task test-acceptance`, `task coverage-cpp`,
`task fuzz`, `task cross-arch` — and the interop cross-check, so run those
separately; on a fresh clone budget ~40 min, since it includes
`task docs-build`.

Beyond the gate, all nine gateways (0–8) are delivered with one verification
guide each, and all **20 rubric NFRs are tracked**, 20 PASS.
`task compliance-report` regenerates that record from a clean clone in one
command — it runs the WCET and BER suites itself, because four of the NFRs
are graded from their JUnit output.

The judge-oriented tour — scoring categories, gateway checklists, the full
NFR table, technical constraints and stated limits — is the
[Evaluation Guide](https://luar-space.github.io/lsis-afs/evaluation_guide.html).

## Verifying the implementation

Every claim below is one command. No manual setup.

```bash
task test-cpp      # ~1 min   C++ unit + integration tests (229 cases)
task test-py       # ~5 s     Python tests (100% coverage gate, 1237 tests)
task swifi         # ~50 s    SWIFI fault-injection campaign (42/42)
task interop       # ~10 s    cross-check against an independent implementation
task compliance-report  # ~3 min  requirement → test traceability, 31/31 and 20/20
task performance   # ~3 min   WCET + constant-time + BER channel simulations
task ci-checks     # ~40 min  the CI gate in one command, offline
```

Times are from a clean clone on an Apple M-series laptop, first run, cold
build included.

### Interoperability

`task interop` clones and builds **LANS-AFS-SIM** — Takuji Ebinuma's
independent LSIS-AFS implementation in C (BSD-2-Clause,
<https://github.com/osqzss/LANS-AFS-SIM>) — then runs the L1–L3 cross-checks
summarised [above](#2-cross-implementation-validation-in-both-directions).
For codes and frames the pass criterion is bit-exact equality, not
statistical agreement.

No third-party source is vendored into this repository. The first run needs
network access to clone LANS-AFS-SIM; every later run is offline, and takes
a few seconds once built. Full methodology, per-level results and stated
limitations are in [docs/interop_results.rst](docs/interop_results.rst).

Reference vectors for other implementations to validate against are published
separately at
[luar-space/lsis-afs-test-vectors](https://github.com/luar-space/lsis-afs-test-vectors)
(with a zero-dependency validator) and can be regenerated here with
`scripts/export_test_vectors_all.py`.

## Examples and notebooks

- [`examples/`](examples/) — minimal scripts: transmit, decode, round-trip
- [`docs/notebooks/`](docs/notebooks/) — tutorial notebooks: spreading codes,
  error correction, navigation message anatomy

## Documentation

The documentation is a deliverable in its own right, not a description of
one. Where to start, by what you want to know:

| Document | What it answers |
|:--|:--|
| [evaluation_guide.rst](docs/evaluation_guide.rst) | *How do I assess this?* Every deliverable → the document that satisfies it → the command that verifies it |
| [mission_assurance.rst](docs/assurance/mission_assurance.rst) | *What space-grade patterns are used, and on whose authority?* The catalogue: SEU-resistant enums, TMR with active repair, variable mirroring, CFI sentinels, the windowed watchdog, Fault Isolation Zones, secure scrub, LUT pinning — each attributed to NASA, ESA or JAXA practice, with the failure it defends against |
| [c_plus_plus_guidelines.rst](docs/assurance/c_plus_plus_guidelines.rst) | *What dialect is the flight code written in?* The C++20 restriction: MISRA C++:2023 unioned with JPL D-60411, Power of Ten, NASA GSFC and N7 Space/ESA, strictest rule winning, every clause tagged with the standards that impose it |
| [pics_proforma.rst](docs/signal/pics_proforma.rst) | *Which protocol features are supported?* 76 items, generated from the shall inventory |
| [shall_inventory.rst](docs/signal/shall_inventory.rst) | *What does the spec require, and what did you do about each one?* All 76 normative identifiers with dispositions |
| [interop_results.rst](docs/interop_results.rst) | *Does it agree with anyone else's implementation?* L1–L3 results, methodology, and what has **not** been run |
| [ldpc_algo_card.rst](docs/signal/ldpc_algo_card.rst) | *How good is the decoder?* Waterfalls, error floor, convergence, quantisation, Shannon-gap decomposition |
| [fmea.rst](docs/assurance/fmea.rst) | *What can fail, and what happens then?* Failure modes with detection and containment |
| [coverage_exclusions.rst](docs/assurance/coverage_exclusions.rst) | *Why isn't coverage higher?* Every exclusion categorised and milestone-reviewed |

Alongside these: quickstart, API reference, per-gateway verification guides
(0–8), the compliance matrix, governance plans (SCMP, SDP, safety plan,
baseline register) and the tutorial notebooks.
`.github/workflows/docs-pages.yml` publishes the set as a GitHub Pages site
when the repository is public; until then, read it locally.

Three ways to read it locally — the `docs/` sources are reStructuredText, so
read them through one of these rather than GitHub's file viewer, which does
not resolve Sphinx cross-references:

| Command | What you get | Time |
|:--|:--|:--|
| `task docs-serve` | the site at <http://127.0.0.1:8000>, auto-reloading | ~15 s |
| `task docs-pdf` | one PDF (`docs/_build/latex/`); needs TeX Live | ~10 s |
| `task docs-build` | static HTML in `docs/_build/html/` | ~35 min fresh, ~15 s after |

`task docs-serve` is the one to use for reading — it needs no notebook
execution and emits no Sphinx warnings. `task docs-build` is the CI gate, not
the reading path: on a fresh clone it first runs the LDPC characterisation
(`task ldpc-algo-card` — a Monte-Carlo campaign whose error-floor points are
60,000 frames each) to produce the algorithm-card figures, then re-executes
every tutorial notebook and builds with `-W` (warnings-as-errors). The
characterisation is where the 35 minutes goes: once
`build/algo_card/sp_results.json` exists, the same command takes ~15 s.

## Provenance

LunaLink was extracted from a larger private development tree. That tree
contains work outside the scope of this release — a multi-signal receiver,
research scripts, unpublished drafts and third-party datasets — none of which
is published here.

The history in this repository is the real development history of the files
that have one. It was produced by filtering the private history down to the
paths that exist here, including their earlier names across the
`signal/ → codes/, encoding/, messages/` restructure, so `git log` and
`git blame` reach back through it. Three things follow from that, and are
worth knowing before reading the log:

- **The commit `LunaLink — LSIS-AFS reference implementation` is the
  extraction point** (2026-08-31). Commits before it come from the private
  tree with non-public paths removed; commits after it were made in this
  repository. The extraction commit is large because it also carries the
  scrub and re-baseline that made the tree publishable.
- **21 commit messages were rewritten**, because they described work that is
  not in this repository. Each says so in a trailer, and each describes the
  change it actually contains. The other 139 are unedited.
- **Files with no private ancestry** — the assurance documents and the
  generated evidence under `docs/assurance/` — were authored here, and their
  history starts at the extraction point.

Historical versions of build configuration (`CMakeLists.txt`, `Taskfile.yml`,
`pyproject.toml`, `.gitignore`) reference directories of the private tree by
name. Nothing from those directories is present in this repository, at any
commit.

The tag `v1.0-submission` marks the competition submission state.

## Licence

Apache-2.0. See [LICENSE](LICENSE) for details.
