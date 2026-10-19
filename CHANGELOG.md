# Changelog

All notable changes to LunaLink are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
follows [Semantic Versioning](https://semver.org/) as described in
`docs/versioning.rst`.

## [Unreleased]

### Added
- `llr_scale` on `frame_decode` and `noise_variance` on `receive_frame`,
  which scale the payload to channel log-likelihood ratios before LDPC
  decoding. Sync detection needs unit-amplitude symbols and the LDPC decoder
  needs LLRs, so one pre-scaled input could not serve both. Both default to
  the previous behaviour.
- Public-release documentation: contributing guide, governance, security
  policy, code of conduct, citation metadata and this changelog.
- `task conformance`, which checks LunaLink against a pinned release of the
  shared test vectors (`luar-space/lsis-afs-test-vectors`) at every level,
  using that repository's own `validate.py diff-*` commands. CI runs it on
  every code change and weekly.
- The API reference lists the binding functions (`prn_code`, `bch_encode`,
  `frame_decode`, `ldpc_decode` and the rest). Sphinx autodoc had skipped all
  of them because it does not recognise nanobind functions.
- `docs/references/verify_references.sh`, which checks the vendored copy of
  the LunaNet specification against the bytes NASA publishes.
- Provenance record for every vendored reference document, in
  `docs/references/README.md`.

### Fixed
- `frame_decode` no longer leaves `crc_ok` set on a subframe whose data it
  has scrubbed. When any subframe failed, the subframes that passed kept
  their flag while their data was zeroed, so a caller could read zeros as
  verified navigation data.
- `receive_frame` raised `ValueError` from the SB2 parser, with either value
  of `raise_on_error`, when SB2 passed its CRC and another subframe failed.
  It now raises `DecodeError` or returns the failed result. The
  `lunalink decode` command had the same crash.
- The Python `frame_decode` binding returned uninitialised memory in the
  data arrays of a failed decode. They are now zero.
- A NaN or infinity in a frame's payload decoded to an all-zero subframe
  that passed LDPC and CRC-24Q, so `frame_decode` reported `kOk` with
  `crc_ok` set and wrong navigation data. It now returns `kInvalidInput`, and
  `ldpc_decode` rejects non-finite LLRs. (FMEA FM-075.)
- `receive_frame` raises `ValueError`, as documented, for a `noise_variance`
  whose LLR scale `2 / noise_variance` overflows or underflows float32,
  instead of failing the decode. It accepts any real number for
  `noise_variance`, including an `int` and the NumPy scalar `np.var`
  returns, which the type check had rejected.
- A large channel LLR (symbol amplitude times `llr_scale`) that overflowed
  float32 made `frame_decode` reject a clean frame; it now saturates.

### Changed
- Repository reorganised for a general audience rather than for competition
  assessment. The competition submission report and evaluation guide are
  preserved under the project history section of the documentation.
- Repository published at https://github.com/luar-space/lunalink. Its
  history was rewritten before publication to remove the competition
  organiser documents from every commit, so commit hashes, including those
  of the `v1.0.0` and `v1.0-submission` tags, differ from any earlier
  private clone.

### Removed
- Competition organiser documents, which were never LunaLink's to redistribute.
- The truncated interleaver and LDPC fixtures in `test-vectors/fec/`. Nothing
  read them, and their schema differed from the shared repository's full
  vectors under the same file names.

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
