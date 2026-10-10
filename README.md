# LunaLink

[![License: Apache 2.0](https://img.shields.io/badge/license-Apache%202.0-green.svg)](LICENSE)
![Python Coverage](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-python-coverage.json)
![Line Coverage](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-line-coverage.json)
![MC/DC Coverage](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-mcdc-coverage.json)

An open-source reference implementation of the **LunaNet Signal-In-Space
Recommended Standard for the Augmented Forward Signal** (LSIS V1.0,
29 January 2025) — the navigation signal that lunar service providers will
broadcast in the 2483.5–2500 MHz band, so that spacecraft and surface assets
at the Moon can fix their position and time the way GNSS receivers do on Earth.

LunaLink implements the standard in both directions, as a C++20 core with
Python bindings:

- **Transmit** — navigation data through frame assembly (CRC-24Q, BCH(51,8),
  LDPC, 60×98 block interleaving), spreading (Gold-2046 and Weil-10230,
  PRN 1–210), BPSK modulation and I/Q multiplexing into a 12-second,
  6000-symbol frame.
- **Receive** — the inverse: frame synchronisation by bipolar
  cross-correlation, soft-decision LDPC and exhaustive maximum-likelihood BCH
  decoding, CRC verification, and navigation message parsing back to typed
  values, with Time of Transmission recovered to code phase.
- **Signal-In-Space Error** computed per LSIS §2.2.2.

No third-party DSP or FEC library is used. The spreading codes, BCH, LDPC,
interleaver, CRC-24Q, BPSK modulation, FFT and sync detector are all
implemented here.

📖 **[Documentation](https://luar-space.github.io/lunalink/)** ·
🧪 **[Test vectors](https://github.com/luar-space/lsis-afs-test-vectors)** ·
🤝 **[Contributing](CONTRIBUTING.md)**

## Install

```bash
git clone https://github.com/luar-space/lunalink
cd lunalink
task install     # builds the C++ core and the Python package, ~1 min
```

You need a C++20 compiler (GCC 13+, Clang 16+, or Apple Clang 15+),
[CMake](https://cmake.org) ≥ 3.15 with Ninja, Python ≥ 3.12 with
[uv](https://docs.astral.sh/uv/), and [Task](https://taskfile.dev). On macOS:
`brew install cmake ninja uv go-task`. [Doxygen](https://www.doxygen.nl) is
needed only to build the docs. A `Dockerfile` is provided if you would rather
not install a toolchain.

Library-only use works with plain pip: `pip install .`

## Quick start

```bash
uv run lunalink generate-codes           # all 210 Gold spreading codes, as hex
uv run lunalink encode --help            # encode a frame or an I/Q signal file
uv run python examples/01_transmit.py    # a 12-s I/Q signal file (~491 MB)
uv run python examples/03_round_trip.py  # transmit → decode, bit-exact check
```

The [quickstart guide](https://luar-space.github.io/lunalink/quickstart.html)
walks through it properly, and the
[tutorial notebooks](https://luar-space.github.io/lunalink/notebooks/01_spreading_codes.html)
cover spreading codes, error correction and the anatomy of a navigation
message.

## Check your implementation against it

If you have an AFS implementation, we want to cross-check against it. There
are three ways to do that.

1. Compare against the shared vectors, without installing LunaLink. The
   vectors for all five interoperability levels are published at
   [lsis-afs-test-vectors](https://github.com/luar-space/lsis-afs-test-vectors),
   with the reference data they were checked against. Its `validate.py` uses
   only the Python standard library and compares your codes, frames, signals,
   decoded bits and parsed data against the set:
   `python validate.py diff-frames /path/to/your/frames/`.
2. Run the cross-checks from LunaLink. `task interop` and
   `task interop-receiver` run it against two independent implementations, and
   `scripts/interop/validate_foreign_vectors.py` runs its decoder over a
   directory of your vectors.
3. Tell us the result. Open an
   [interoperability report](https://github.com/luar-space/lunalink/issues/new?template=interop_report.yml),
   passing or failing.

The [interoperability guide](https://luar-space.github.io/lunalink/interoperate.html)
has the commands for each level.

## Why this implementation

If you have fifteen minutes, this is the tour:

```bash
task interop      # ~10 s  bit-exact against an independent implementation
task swifi        # ~50 s  42/42 fault-injection test cases
task test-cpp && task test-py   # ~1 min  232 C++ cases, 1250 Python tests
```

Every claim below has a command beside it. Nothing here is asserted.

### It interoperates, in both directions, and that is checked on every run

Not a diff against stored vectors. LunaLink **acquires and decodes another
implementation's signal**, and another implementation decodes LunaLink's.

Against [LANS-AFS-SIM](https://github.com/osqzss/LANS-AFS-SIM), an independent
C implementation, via `task interop`:

| Level | Check | Result |
|:--|:--|:--|
| L1 | Gold-2046 + Weil-10230 codes, PRN 1–210 | 0 chip mismatches |
| L2 | Frame encoding, 5 cases (BCH, CRC-24, LDPC, interleaver) | 0/6000 symbol mismatches |
| L3 | Acquisition, tracking and full decode of a 25 s, 7-satellite composite signal with lunar near-rectilinear halo orbit ephemeris | all 3 subframes CRC-24 verified |

The two implementations derive spreading codes by **structurally different
methods**, an 11-bit shift register with a delay table versus tables
precomputed from the appendices. Bit-exact agreement is therefore independent
confirmation, not two programs reading one table.

`task interop-receiver` runs the other direction:
[PocketSDR-AFS](https://github.com/osqzss/PocketSDR-AFS), an independent
software-defined receiver, acquires, tracks, decodes and CRC-verifies a signal
LunaLink generated.

```
transmitted SB2: 4850D19D04E4308D820462770927D9CA…  crc=A939B3
decoded     SB2: 4850D19D04E4308D820462770927D9CA…  crc=A939B3
```

No third-party source is vendored here; the interop tasks clone and build the
counterparts. Method, limits and per-level results are in the
[interoperability report](https://luar-space.github.io/lunalink/interop_results.html).

### It is written as flight software, not as a simulation

The dialect is a restricted C++20 that unions MISRA C++:2023 (179 guidelines)
with the JPL Institutional Coding Standard (D-60411), Holzmann's Power of Ten,
the NASA Goddard C++ standards and the ESA-commissioned N7 Space C++20 study,
**taking the strictest rule wherever two overlap**. Every rule carries its
provenance inline: the `[[nodiscard]]` mandate reads
`[MISRA 0.1.2; JPL Rule 14; P10 Rule 7]`, so any restriction traces back to
the standard that imposes it.

In practice: no exceptions, no RTTI, no heap after initialisation, no
recursion, fixed-bound loops so worst-case stack depth is computable,
cyclomatic complexity ≤ 10, single-exit DSP functions so buffer scrubbing
cannot be skipped by an early return, and six *forbidden* optimisation flags
that `task check-flags` fails the build over.

On top of that dialect sits a layered defence:

- **Data integrity** — SEU-resistant enums using non-contiguous,
  high-Hamming-distance encodings, so a bit flip yields an *illegal* value
  rather than a plausible one; triple modular redundancy with active repair;
  mirrored loop variables; CRC-32 self-validating lookup tables.
- **Control-flow integrity** — sentinels, a legal-transition matrix,
  forbidden-state detection, and a **windowed** watchdog whose *lower* bound
  catches a radiation-induced jump over a loop.
- **Configuration integrity** — a Software Configuration Item header (git SHA,
  specification version, build timestamp, CRC-32) linked into the binary and
  verified as step 1 of the power-on self-test. Nothing transmits a symbol
  until the build verifies itself.
- **Containment** — three nested Fault Isolation Zones, annotated in-source,
  bounding the blast radius of any fault. The guarantee: a single radiation
  event can corrupt at most one 12-second navigation frame, never the timing
  system, never the on-board computer.

Each pattern is attributed to its origin in NASA, ESA or JAXA flight practice
in the [mission assurance case](https://luar-space.github.io/lunalink/assurance/mission_assurance.html),
and each is exercised by fault injection rather than asserted. `task swifi`
drives 34 scenarios, in 42 test cases across six techniques, against the real
injection APIs, all compiled out of flight builds.

C++ line coverage is 90.27% **by design**: 123 in-source
`NOCOVER(DEFENCE_IN_DEPTH)` annotations mark paths that execute only when a
cosmic ray flips a bit. Those are covered by fault injection instead, and
every exclusion is categorised and reviewed in the
[exclusions register](https://luar-space.github.io/lunalink/assurance/coverage_exclusions.html).

### The decoder is measured against theory, not just "it works"

An independently written, unhardened float64 sum-product decoder exists purely
as the theoretical ceiling. Comparing the shipped decoder against it *and*
against the finite-length sphere-packing bound splits the Shannon gap into two
attributable halves: the **code gap**, inherent to the design, and the
**implementation gap** from hardening and clamping. The shipped, hardened
decoder lands **within 0.15 dB of reference belief propagation**. The ~1.0 dB
total gap belongs to the code, not to this implementation.

Beyond that: bit and frame error waterfalls with 95% Wilson confidence
intervals; an error-floor search finding zero errors in 60,000 frames per
point; a convergence knee at ~15 iterations against a shipped cap of 50; and a
quantisation sweep showing **int8 costs no measurable performance**, so an
8-bit front end suffices. That last one is a receiver-design conclusion, not
just a test result. `task ldpc-algo-card` regenerates all of it.

### The tools are qualified, not trusted

Most projects assert their analysers work. These are made to prove it against
deliberately defective code:

```bash
task validate-tidy      # proves clang-tidy catches the MISRA violations claimed
task validate-coverage  # proves llvm-cov correctly reports known gaps
```

Three static analysers run (clang-tidy on a MISRA-traceable profile, cppcheck,
Clang Static Analyzer) and three runtime checkers (AddressSanitizer,
UndefinedBehaviorSanitizer, libFuzzer with a committed seed corpus and a
nightly deep fuzz). MC/DC is measured natively. CI runs on **big-endian
s390x** alongside arm64, catching byte-order defects that x86 and ARM CI never
will.

### The evidence regenerates itself

Every artefact is script-produced rather than maintained by hand, and each has
the command that regenerates it:

| Command | What it establishes |
|:--|:--|
| `task compliance-report` | Requirement and non-functional verdicts, from this run's output |
| `task traceability` | Requirement → test tags, and that none is orphaned |
| `task coverage-cpp` | Region, line and MC/DC coverage with per-file waivers |
| `task swifi` | 34 fault-injection scenarios, in 42 test cases across 6 techniques |
| `task swifi-reconcile` | Every fault-injection anchor matches the FMEA table, in both directions |
| `task pics-proforma` | Conformance statement, failing if it disagrees with the shall inventory |
| `task stack-usage` | Worst-case stack depth |
| `task build-manifest` | Toolchain, flags and dependency hashes for the binary |
| `task check-doc-metrics` | That documented figures still match the evidence they came from |
| `task validate-export` | That the **committed** tree builds and tests, not just the working directory |

The last two keep the rest honest. One fails if a number in the documentation
drifts from its artefact; the other builds from `git archive` rather than from
disk, catching anything present locally but never committed.

`task ci-checks` reproduces the whole CI gate locally and offline, in about
forty minutes.

### It feeds back to the standard

LSIS V1.0 lists 46 open items in its own Table ZZ-1: 16 *To Be Written*, 25
*To Be Confirmed* and 5 *To Be Determined*. Every one is dispositioned in the
[specification findings report](https://luar-space.github.io/lunalink/signal/spec_findings_report.html#open-items).
Most are tolerances on radio-frequency hardware. The five that are
**interoperability-critical**, where two conformant implementations can make
different choices and fail to interoperate, are written up there as findings
SF-001 to SF-005, each with the assumption adopted, a proposed resolution,
and a recommendation to the working group.

For the 11 message types whose payload the specification leaves open, LunaLink
proposes concrete bit-level layouts, so that two implementations can agree on
something real rather than on "to be written".

**Read the standard differently than we did?**
[Tell us](https://github.com/luar-space/lunalink/issues/new?template=spec_finding.yml).
These are the most valuable issues this project receives.

## Status and limits

LunaLink is a complete, validated implementation of the AFS signal chain. It
has **not flown**, has not been through independent software validation and
verification, and is not qualified against ECSS-Q-ST-80C for any mission. ECSS
Category B is self-assessed here, not certified.

Known gaps, stated rather than discovered:

- **No positioning pipeline.** Constellation simulation and position fix are
  future work.
- **No hardware in the loop.** The hardware abstraction layer coverage waivers
  resolve only on target hardware.
- **No clock-drift compensation.** The receiver has no delay-locked loop.
  Sample-clock-offset and phase-noise sweeps are named in the validation plan
  and not implemented.
- **Interoperability breadth.** Both directions are demonstrated, but against
  generated baseband rather than captured radio-frequency signals, and SB3 and
  SB4 comparison needs a receiver that logs them decoded.

The [roadmap](https://luar-space.github.io/lunalink/roadmap.html) tracks these.
Where something is bounded, the documentation says so, and the positive claims
above are meant to be read in that light.

## Documentation

The full documentation site is at
**<https://luar-space.github.io/lunalink/>**. Start with the
[quickstart](https://luar-space.github.io/lunalink/quickstart.html), then:

| Document | What it answers |
|:--|:--|
| [Architecture](https://luar-space.github.io/lunalink/signal/architecture.html) | How does the signal chain fit together? |
| [Mission assurance](https://luar-space.github.io/lunalink/assurance/mission_assurance.html) | What space-grade patterns are used, and on whose authority? |
| [C++ guidelines](https://luar-space.github.io/lunalink/assurance/c_plus_plus_guidelines.html) | What dialect is the flight code written in, and which standard imposes each rule? |
| [Interoperability](https://luar-space.github.io/lunalink/interop_results.html) | Does it agree with anyone else's implementation? |
| [LDPC algorithm card](https://luar-space.github.io/lunalink/signal/ldpc_algo_card.html) | How good is the decoder, against theory? |
| [Shall inventory](https://luar-space.github.io/lunalink/signal/shall_inventory.html) | What does the standard require, and what was done about each item? |
| [FMEA](https://luar-space.github.io/lunalink/assurance/fmea.html) | What can fail, and what happens then? |
| [API reference](https://luar-space.github.io/lunalink/api_cpp.html) | 434 C++ entities and 162 Python symbols, generated from the source |

To read it locally, `task docs-serve` puts it on
<http://127.0.0.1:8000> in about fifteen seconds. `task --list` prints every
task with a description, which is the complete inventory of what this
repository can verify about itself.

## Contributing

Contributions are welcome, especially specification findings,
interoperability results from other implementations, test vectors that
disagree with ours, and ports to new targets. See
[CONTRIBUTING.md](CONTRIBUTING.md) for the toolchain and for the rules that
are stricter here than in most projects.

- [Code of conduct](CODE_OF_CONDUCT.md)
- [Governance](GOVERNANCE.md) — who decides what, and how that should change
- [Security policy](SECURITY.md) — report vulnerabilities privately

## Provenance

LunaLink was extracted from a larger private development tree that contains
work outside the scope of this release, none of which is published here.

The history in this repository is the real development history of the files
that have one, filtered down to the paths that exist here, including their
earlier names across an internal restructure, so `git log` and `git blame`
reach back through it. Three things follow:

- The commit *LunaLink — LSIS-AFS reference implementation* (2026-08-31) is
  the extraction point. Commits before it come from the private tree with
  non-public paths removed; commits after it were made here. The extraction
  commit is large because it carries the scrub that made the tree publishable.
- **21 commit messages were rewritten**, because they described work not in
  this repository. Each says so in a trailer and describes the change it
  actually contains. The rest are unedited.
- Files with no private ancestry, such as the assurance documents, were
  authored here and their history starts at the extraction point.

Historical versions of the build configuration reference directories of the
private tree by name. Nothing from those directories is present here, at any
commit.

## Acknowledgements

LunaLink was built for the **ESA-CCSDS Protocol Development Outreach
Competition** on the LSIS-AFS track, which it won in September 2026. The
submission report and the evaluation guide written for the judges are
preserved in the
[project history](https://luar-space.github.io/lunalink/history/index.html).
That result is a statement about the work at a point in time and not a
designation of official status; LunaLink is not endorsed by NASA, ESA, CCSDS
or any LunaNet service provider.

The specification is NASA's, published as part of the
[LunaNet Interoperability Specification](https://www.nasa.gov/directorates/somd/space-communications-navigation-program/lunanet-interoperability-specification/).
Interoperability testing would not have been possible without the independent
implementations LANS-AFS-SIM and PocketSDR-AFS.

## Licence

Apache-2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

Reference documents under `docs/references/` are United States Government
works published by NASA and are not covered by that licence. See
[docs/references/README.md](docs/references/README.md) for provenance.
