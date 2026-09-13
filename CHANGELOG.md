# Changelog

All notable changes to LunaLink are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
follows [Semantic Versioning](https://semver.org/) as described in
`docs/versioning.rst`.

## [Unreleased]

### Added
- Public-release documentation: contributing guide, governance, security
  policy, code of conduct, citation metadata and this changelog.
- `docs/references/verify_references.sh`, which checks the vendored copy of
  the LunaNet specification against the bytes NASA publishes.
- Provenance record for every vendored reference document, in
  `docs/references/README.md`.

### Changed
- Repository reorganised for a general audience rather than for competition
  assessment. The competition submission report and evaluation guide are
  preserved under the project history section of the documentation.

### Removed
- Competition organiser documents, which were never LunaLink's to redistribute.

## [1.0.0] - 2026-09-04

First complete implementation, submitted to and winner of the ESA-CCSDS
Protocol Development Outreach Competition.

### Added
- Complete AFS transmit chain: Gold-2046 and Weil-10230 spreading codes for
  PRN 1-210, CRC-24Q, BCH(51,8), LDPC for SF2 (2400,1200) and SF3/SF4
  (1740,870), 60x98 block interleaving, BPSK modulation and I/Q multiplexing
  into a 12-second, 6000-symbol frame.
- Complete receive chain: frame synchronisation by bipolar cross-correlation,
  soft-decision LDPC and exhaustive maximum-likelihood BCH decoding, CRC
  verification, navigation message parsing, and Time of Transmission recovery
  to code phase.
- Signal-In-Space Error computation per LSIS section 2.2.2.
- C++20 core with Python bindings via nanobind.
- Flight-software assurance case: SEU-resistant enums, triple modular
  redundancy with active repair, control-flow integrity sentinels, a windowed
  watchdog, a Software Configuration Item header verified at power-on, and
  three nested fault isolation zones.
- Software FMEA reconciled against fault-injection scenarios, with 42 SWIFI
  scenarios across six techniques.
- Bidirectional interoperability evidence against two independent
  implementations.
- Documentation site covering architecture, assurance, compliance and
  specification findings.

[Unreleased]: https://github.com/luar-space/lunalink/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/luar-space/lunalink/releases/tag/v1.0.0
