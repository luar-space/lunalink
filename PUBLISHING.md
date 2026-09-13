# Publication runbook

How to take this repository public as `luar-space/lunalink`. Everything up to
the push is done; this file is the part that needs a human.

Delete this file once the repository is published. It is scaffolding, not
documentation.

---

## Decide first

**1. The patent grant.** Apache-2.0 §3 grants a patent licence to everyone who
receives the code. It is irrevocable and cannot be walked back after
publication. If anything in the signal chain is patentable, and the plausible
candidates are the exhaustive maximum-likelihood BCH decoder and the
windowed-watchdog plus fault-isolation-zone arrangement, decide *before* you
push. Not deciding is itself a decision to grant.

**2. Export control.** The specification is public and NASA-published, and the
implementation is of a civil navigation signal, so this is very likely
unproblematic. It is still worth a recorded assessment before a European
entity publishes flight-grade signal-processing code, rather than after.

**3. Contribution terms.** `CONTRIBUTING.md` currently specifies the Developer
Certificate of Origin and no contributor licence agreement, so contributors
keep their copyright and you cannot unilaterally relicense later. That is the
right default for adoption and for eventual donation to a standards body. If
you would rather keep relicensing headroom, change it now, because changing it
after contributors arrive means asking each of them.

## Do not push these tags

Two tags point at 19 commits that are **not reachable from `main`**:

```
pre-graft-backup
pre-graft-v1.0-submission
```

They are local backups from a history rewrite. They carry paths that exist
nowhere on `main`, including a hardware fingerprint file and draft material.
Publishing them would put a parallel history into the public repository for no
benefit.

**Therefore: never use `git push --tags`, `--mirror`, or `--follow-tags` on
this repository.** Push the two release tags by name, as below. The backup
tags stay on this machine, which is where they are useful.

## Publish

```bash
# 1. Create the repository. Public from the start: creating it private and
#    flipping it later leaves the private-era forks and clones behind.
gh repo create luar-space/lunalink \
  --public \
  --description "Open reference implementation of the LunaNet Augmented Forward Signal (LSIS V1.0)" \
  --homepage "https://luar-space.github.io/lunalink/"

# 2. Point this clone at it. The existing "submission" remote stays as it is.
git remote add origin git@github.com:luar-space/lunalink.git

# 3. Push main, and only the two release tags.
git push -u origin main
git push origin v1.0.0 v1.0-submission
```

## Configure, in this order

```bash
# Documentation site. docs-pages.yml already skips the deploy when Pages is
# off, so nothing failed while the repo was private; this switches it on.
gh api -X POST repos/luar-space/lunalink/pages \
  -f 'build_type=workflow' || true

# Private vulnerability reporting, which SECURITY.md tells people to use.
gh api -X PUT repos/luar-space/lunalink/private-vulnerability-reporting

# Branch protection on main.
gh api -X PUT repos/luar-space/lunalink/branches/main/protection \
  --input - <<'JSON'
{
  "required_status_checks": {"strict": true, "contexts": []},
  "enforce_admins": false,
  "required_pull_request_reviews": null,
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false
}
JSON
```

Then in the web interface:

- **Topics**: `lunanet`, `afs`, `lunar-navigation`, `gnss`, `ccsds`, `sdr`,
  `ldpc`, `flight-software`, `space`, `cpp20`.
- **Repository variable** `COVERAGE_GIST_ID`, set to the gist the coverage
  badges write to. Without it the badge steps skip, which is harmless but
  leaves the README badges stale.
- **Secret** `GIST_SECRET`, the token for that gist. Badge steps already
  guard on it being present, so forks will not fail.
- Disable Wikis and Projects unless you intend to use them. An empty wiki tab
  invites issues filed in the wrong place.

## First release

PyPI publication uses trusted publishing, so there is no token to store.
Configure the pending publisher once at
<https://pypi.org/manage/account/publishing/>:

| Field | Value |
|:--|:--|
| Project | `lunalink` |
| Owner | `luar-space` |
| Repository | `lunalink` |
| Workflow | `release.yml` |
| Environment | `pypi` |

Then create the GitHub release for `v1.0.0`, which triggers `release.yml` to
build an sdist plus native wheels on five runners and publish them.

Consider running the release workflow via `workflow_dispatch` first. It builds
everything and skips both publish jobs, so a wheel build failure surfaces
before a tag is public.

## Announce, in roughly this order

1. **The interoperability counterparts first.** The author of LANS-AFS-SIM and
   PocketSDR-AFS made the cross-validation evidence possible and is named
   throughout. Tell them before the world.
2. **The competition organisers.** Ask for an introduction to the relevant
   CCSDS Space Link Services working group while the result is fresh. This is
   the single highest-leverage thing in this file and it decays with time.
3. **The specification authors.** Findings SF-001 to SF-005 are the five
   interoperability-critical ambiguities. Send them as formal comments rather
   than as a link to a repository. Being the implementer who reports the
   specification's defects is how you become the person its authors call.
4. **Then the public**: Zenodo for a DOI, which `CITATION.cff` is ready for,
   and wherever lunar navigation people actually read.

## Known follow-ups

- **Zenodo DOI.** `CITATION.cff` has no `doi:` field yet. Add one after the
  first archived release.
- **A security contact address.** `SECURITY.md` routes through GitHub private
  reporting, which is fine, but a `security@` address on your own domain reads
  better and survives a platform change.
- **Notebook churn.** `task docs-build` re-executes the tutorial notebooks and
  rewrites their committed outputs, so unrelated commits pick up hundreds of
  lines of notebook diff. Consider stripping outputs on commit.
- **Bus factor.** `GOVERNANCE.md` states it plainly and commits to widening the
  maintainer group. It is the first thing a serious institutional adopter will
  raise.
