## What this changes

<!-- One paragraph. What behaviour differs after this merges? -->

## Why

<!-- If this changes the signal chain, cite the clause of LSIS V1.0 that
     requires it. "It seemed more correct" is not a basis for divergence. -->

## Checks

- [ ] `task lint` passes
- [ ] `task test` passes
- [ ] Commits are signed off (`git commit -s`), per [CONTRIBUTING.md](../CONTRIBUTING.md)

## If this touches `cpp/`

- [ ] Conforms to `docs/assurance/c_plus_plus_guidelines.rst`: no exceptions,
      no RTTI, no allocation after init, no recursion, bounded loops,
      cyclomatic complexity at most 10
- [ ] `task tidy` passes
- [ ] Any path unreachable by normal testing is annotated `NOCOVER(...)` and has
      a matching fault-injection scenario, and `task swifi-reconcile` passes

## If this changes behaviour a requirement covers

- [ ] `docs/assurance/traceability_matrix.rst` updated in this pull request
- [ ] `task traceability` passes

## If this changes the public API

- [ ] `CHANGELOG.md` updated
- [ ] Version impact noted: patch / minor / major
