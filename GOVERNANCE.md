# Governance

LunaLink is maintained by [Luar Space](https://github.com/luar-space). This
document says who decides what, and how that is expected to change.

## Current state: single maintainer

The project has one maintainer, Marco Fiscato, who wrote the implementation and
retains final say on what merges. Pretending otherwise would be theatre. This
is stated plainly so that anyone evaluating LunaLink for serious use knows
exactly how thin the bus factor is today.

## How decisions are made

- **Ordinary changes.** Open a pull request. It needs to pass CI and satisfy
  the rules in [CONTRIBUTING.md](CONTRIBUTING.md).
- **Behavioural changes to the signal chain.** These need a stated
  specification basis. A change that makes LunaLink diverge from LSIS V1.0 will
  not merge unless it is fixing a place where LunaLink was already wrong, and
  the pull request says which clause it is reading differently and why.
- **Specification ambiguities.** Where the standard is genuinely open, the
  interpretation is recorded in `docs/signal/spec_findings_report.rst` with the
  reasoning, and raised with the standard's authors rather than settled
  silently in a header file. Interpretations can change; the record of why they
  were made does not get rewritten.
- **Breaking API changes.** Major version bump, with the rationale in the
  changelog. See `docs/versioning.rst`.

## Becoming a maintainer

There is no committee to join yet. Sustained, high-quality contribution is the
route: several merged pull requests of substance, demonstrated judgement about
where the specification binds and where it does not, and a willingness to
review other people's work. If that describes you, say so in an issue.

The intent is to move to a small group of maintainers with commit rights, and
to publish the list here, rather than to keep this concentrated.

## Relationship to the standard

LunaLink implements the LunaNet Signal-In-Space Recommended Standard for the
Augmented Forward Signal. It does not define it. NASA and its LunaNet partners
own the specification; this project is downstream of it.

That has two consequences worth stating.

First, when LunaLink and the specification disagree, the specification wins and
LunaLink has a bug. The only exception is a defect in the standard itself, and
the response to that is to report it upward, not to route around it.

Second, LunaLink is not endorsed by NASA, ESA, CCSDS, or any LunaNet service
provider. It was built for the ESA-CCSDS Protocol Development Outreach
Competition and won it, which is a statement about the work and not a
designation of official status. If that ever changes, it will be recorded here
with a citation, and not implied by adjective.

## Neutrality

LunaLink is Apache-2.0 and intends to stay that way. Luar Space maintains it
and may offer commercial services around it, such as integration, target ports
and qualification support. Those services will not be built on features held
back from the open repository. If the protocol implementation, its test suite
and its assurance documentation ever start diverging from what is published
here, the project has failed at the thing it is for.

Should LunaLink be adopted as a reference implementation by a standards body or
a mission programme, transferring stewardship to a neutral foundation is the
expected outcome, not a concession.

## Contact

Open an issue for anything public. Security reports go through
[SECURITY.md](SECURITY.md).
