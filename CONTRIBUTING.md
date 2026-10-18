# Contributing to LunaLink

Thanks for considering a contribution. LunaLink is a reference implementation
of a public standard, so the bar for changes is unusual: correctness is judged
against the specification, not against what seems reasonable.

Read [the developer guide][dev] for the full toolchain. This file covers what
is specific to contributing.

## Ways to contribute that we especially want

- **Specification findings.** If you read LSIS V1.0 and reach a different
  interpretation than this code does, that is the most valuable issue you can
  file. Use the *Specification finding* issue template. Several open items in
  the standard are marked TBC/TBD, and where LunaLink had to choose, it records
  the choice and the reasoning in `docs/signal/spec_findings_report.rst`.
- **Independent interoperability results.** If you have another AFS
  implementation, transmitter or receiver, and you cross-check it against
  LunaLink, we want the result whether it passes or fails. Failures are more
  useful.
- **Test vectors.** Vectors that disagree with the shared set, or that cover
  cases it does not, belong in [lsis-afs-test-vectors][vectors].
- **Ports.** Building the core for a new target, especially real flight-adjacent
  hardware, closes gaps that no amount of desktop testing can.

## Before you open a pull request

```bash
task install        # build the C++ core and the Python package
task install-hooks  # register the pre-commit hook
task lint           # pyright and ruff
task test           # C++ Catch2 tests, then pytest
```

`task ci-checks` runs every CI workflow locally and offline. It takes about
forty minutes. Running it before a large change saves a round trip.

## Rules that are stricter here than in most projects

The signal chain under `cpp/` is written as flight software, not as a
simulation. `docs/assurance/c_plus_plus_guidelines.rst` is the normative
statement; the short version is:

- No exceptions, no RTTI, no heap allocation after initialisation, no recursion.
- Fixed-bound loops, so worst-case stack depth stays computable.
- Cyclomatic complexity at most 10.
- Single-exit DSP functions, so buffer scrubbing cannot be skipped by an early
  return.
- `[[nodiscard]]` and `noexcept` where the guidelines require them.

Every restriction in that document carries the standard that imposes it, inline.
If a rule blocks something you need, quote the rule in the pull request and
argue with it. Do not work around it quietly.

Python under `src/` and `tests/` is ordinary modern Python and is held to 100%
line coverage, enforced in CI.

## Coverage and untestable paths

Some C++ paths execute only when a cosmic ray flips a bit, and cannot be
reached by a normal test. Those are marked in-source with
`NOCOVER(DEFENCE_IN_DEPTH)` and are covered instead by fault injection
(`task swifi`). If you add such a path, annotate it and add the matching
injection scenario. `task swifi-reconcile` fails the build on an annotation
with no scenario, or a scenario with no annotation, in either direction.

## Traceability

Requirements, code and tests are linked. If you change behaviour that a
requirement covers, update the matrix in `docs/assurance/traceability_matrix.rst`
in the same pull request. `task traceability` checks the links.

## Commit sign-off

LunaLink uses the [Developer Certificate of Origin][dco]. It is a statement
that you wrote the contribution, or otherwise have the right to submit it under
Apache-2.0. Sign each commit:

```bash
git commit -s -m "your message"
```

There is no contributor licence agreement and no copyright assignment. You keep
copyright in what you write.

## Code of conduct

Participation is governed by [the code of conduct](CODE_OF_CONDUCT.md).

[dev]: https://luar-space.github.io/lunalink/developing.html
[vectors]: https://github.com/luar-space/lsis-afs-test-vectors
[dco]: https://developercertificate.org/
