# LunaLink — LSIS-AFS Reference Implementation

Open-source reference implementation of the **LunaNet Signal-In-Space
Augmented Forward Signal (LSIS-AFS)** protocol, as specified in LSIS V1.0
(29 January 2025).

Built as part of the **ESA-CCSDS Protocol Development Outreach Competition**.

## Features

- Complete AFS signal chain: spreading codes, FEC (BCH + LDPC), framing,
  BPSK modulation, IQ multiplexing
- Software correlator with PCPS acquisition and DLL/PLL tracking
- Frame sync detector with bipolar cross-correlation
- C++20 core with Python bindings (nanobind)
- Deterministic, branchless signal processing (`-fno-exceptions -fno-rtti
  -fno-fast-math`)
- Comprehensive test suite: unit tests, BER simulations, round-trip
  verification, sanitizer and coverage gates

## Quick Start

```bash
pip install lunalink
```

Or build from source:

```bash
git clone https://github.com/luar-space/lsis-afs.git
cd lsis-afs
pip install -e ".[dev]"
```

## Verifying the implementation

Every claim below is one command. No manual setup.

```bash
task test-cpp    # C++ unit + integration tests
task test-py     # Python tests (100% coverage gate)
task swifi       # SWIFI fault-injection campaign
task interop     # cross-check against an independent implementation
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
takes a few seconds once built.

Reference vectors for other implementations to validate against are published
separately, with a zero-dependency validator, and can be regenerated here with
`scripts/export_test_vectors_all.py`.

## Documentation

Full documentation is available at the project's GitHub Pages site.

## Licence

Apache-2.0. See [LICENSE](LICENSE) for details.
