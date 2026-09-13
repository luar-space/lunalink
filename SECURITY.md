# Security policy

## Reporting a vulnerability

Report security issues privately through GitHub's
[private vulnerability reporting][gh] on this repository. Do not open a public
issue for a vulnerability.

Expect an acknowledgement within five working days and an assessment within
fifteen. If a fix is warranted we will agree a disclosure date with you, and
credit you in the advisory unless you ask us not to.

[gh]: https://github.com/luar-space/lunalink/security/advisories/new

## What counts as a vulnerability here

LunaLink processes untrusted input in one place that matters: the decode chain
consumes sampled signal data and navigation frames that an attacker may control.
Treat the following as in scope.

- Memory-safety defects in the C++ core reachable from decode, including the
  frame synchroniser, the LDPC and BCH decoders, the deinterleaver and the
  message parser. These are the fuzz targets for a reason.
- Any input that causes the core to allocate after initialisation, recurse,
  or exceed a documented worst-case execution time budget. In flight software a
  timing violation is a safety property, not a performance complaint.
- Bypasses of the integrity mechanisms: the Software Configuration Item header
  check, the CRC-32 self-validating tables, the state-machine transition matrix
  or the fault isolation zone boundaries.
- Incorrect decoding that a crafted input could exploit to make a receiver
  accept a navigation message it should reject.

## What does not count

- Defects that require modifying LunaLink's own source or build flags first.
- Faults injected through the SWIFI harness. That code is deliberately
  destructive, exists to break invariants on purpose, and is compiled out of
  release builds.
- Resource use in the Python tooling, notebooks, benchmarks or test harness.
  None of that is on a flight path.
- Divergence from the specification, unless it has a security consequence.
  Those are ordinary bugs and belong in a public issue, where they are more
  useful to everyone.

## Supported versions

LunaLink is pre-1.0 in its public form. Security fixes land on `main` and in
the next release. There is no long-term-support branch yet; when the first
release is designated as a baseline for an external programme, this section
will say so.

## A note on flight use

Nothing here has flown. LunaLink is written to a flight-software dialect and
carries an assurance case, but it has not been through independent software
validation and verification, nor qualified against ECSS-Q-ST-80C for any
mission. Anyone considering it for an operational system should read
`docs/assurance/` and draw their own conclusions, and should not treat this
policy as a substitute for their own programme's anomaly reporting.
