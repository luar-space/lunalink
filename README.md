# LunaLink — LSIS-AFS Reference Implementation

Open-source reference implementation of the **LunaNet Signal-In-Space
Augmented Forward Signal (LSIS-AFS)** protocol, as specified in LSIS V1.0
(29 January 2025).

Built as part of the **ESA-CCSDS Protocol Development Outreach Competition**.

## Features

- Complete AFS signal chain: spreading codes, FEC (BCH + LDPC), framing,
  BPSK modulation, IQ multiplexing — and the inverse decode chain back to
  parsed navigation data
- Frame sync detector with bipolar cross-correlation
- C++20 core with Python bindings (nanobind)
- Deterministic, branchless signal processing (`-fno-exceptions -fno-rtti
  -fno-fast-math`)
- Fault-hardened to flight-software norms: CFI sentinels, TMR mirroring,
  bounds-checked tables, SWIFI fault-injection campaign, FMEA + MC/DC
  coverage evidence
- Comprehensive test suite: unit tests, Annex 3 bit-exact reference checks,
  BER simulations, round-trip verification, sanitizer and coverage gates

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
git clone https://github.com/luar-space/lsis-afs.git
cd lsis-afs
task install        # uv sync --group dev: builds the C++ core + Python package
```

Then generate the competition artefacts directly from the CLI:

```bash
uv run lunalink generate-codes           # all 210 PRN spreading codes (hex)
uv run lunalink encode --help            # encode a 12-s frame or I/Q signal file
uv run python examples/01_transmit.py    # 12-s I/Q signal file (writes ~491 MB)
uv run python examples/03_round_trip.py  # transmit → decode, bit-exact check
```

Library-only use (no dev tools) also works with plain pip: `pip install .`

## For evaluators

Each pass/fail gate item is one command, run from the repo root after
`task install`:

| Gate item | Command | Evidence |
|:--|:--|:--|
| ≥ 12 PRN spreading codes | `uv run lunalink generate-codes` | 210 codes, bit-exact vs Annex 3 (`tests/test_annex3_codes.py`) |
| Complete 12-second frame | `uv run python examples/01_transmit.py` | 6000-symbol frame, all subframes |
| I/Q signal files | same command | `LSISIQ`-headed float32 file; short vectors in `test-vectors/signals/` |
| Validation tests | `task test-cpp && task test-py` | full unit/integration suites |
| Setup/build instructions | this file + [docs/quickstart.rst](docs/quickstart.rst) | — |

The judge-oriented tour of every deliverable, with per-gateway verification
commands, is [docs/evaluation_guide.rst](docs/evaluation_guide.rst).

## Verifying the implementation

Every claim below is one command. No manual setup.

```bash
task test-cpp      # C++ unit + integration tests
task test-py       # Python tests (100% coverage gate)
task swifi         # SWIFI fault-injection campaign
task interop       # cross-check against an independent implementation
task performance   # BER waterfalls + WCET timing acceptance (longer run)
task compliance-report  # spec-requirement → test traceability report
```

### Interoperability

`task interop` clones and builds **LANS-AFS-SIM** — Takuji Ebinuma's
independent LSIS-AFS implementation in C (BSD-2-Clause,
<https://github.com/osqzss/LANS-AFS-SIM>) — then compares LunaLink against it
bit-exactly:

| Check | Scope |
|:--|:--|
| PRN spreading codes | Gold + Weil, every chip of PRN 1–210 |
| Encoded frames | BCH + CRC-24 + LDPC + interleaver, all 6000 symbols |
| Self-consistency | against the published test vectors |

Pass criterion is bit-exact equality — not statistical agreement. The run
takes a few seconds once built. **Note:** the first run needs network access
to clone LANS-AFS-SIM; every later run is offline. Results are documented in
[docs/interop_results.rst](docs/interop_results.rst).

Reference vectors for other implementations to validate against are published
separately at
[luar-space/lsis-afs-test-vectors](https://github.com/luar-space/lsis-afs-test-vectors)
(with a zero-dependency validator) and can be regenerated here with
`scripts/export_test_vectors_all.py`.

## Examples and notebooks

- [`examples/`](examples/) — minimal scripts: transmit, decode, round-trip
- [`docs/notebooks/`](docs/notebooks/) — tutorial notebooks: spreading codes,
  error correction, navigation message anatomy
- [`demo/`](demo/) — signal-chain visualisation

## Documentation

Build the full Sphinx site locally with `task docs-build` (output in
`docs/_build/html/`), or browse the sources in [docs/](docs/): quickstart,
API reference, per-gateway deliverable verification, compliance matrix,
assurance evidence (FMEA, fault coverage, MC/DC), and interoperability
results.

## Provenance

This repository is the public release of the LunaLink development tree,
exported as a single squashed commit; development history (branch/PR flow,
CI gates on every merge) predates the export. The tag `v1.0-submission`
marks the competition submission state.

## Licence

Apache-2.0. See [LICENSE](LICENSE) for details.
