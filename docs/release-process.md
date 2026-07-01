# Release Process

Tickline releases are engineering checkpoints. A release should describe a real, verifiable state of the repository, not an aspirational plan.

A release is allowed only when the code, documentation, tests, GitHub issues, and public claims agree with each other.

## Release principle

```text
a release records what is true now
not what the project hopes to become later
```

This matters because Tickline is a portfolio project. Overclaiming damages credibility more than a smaller but accurate release.

## Version model

Tickline uses semantic-looking milestone versions, but before `v1.0.0` the version numbers represent portfolio milestones rather than public API stability.

Expected line:

```text
v0.1.0  blueprint and engineering skeleton
v0.2.0  deterministic simulation core
v0.3.0  protocol parser and hardening
v0.4.0  authoritative validation
v0.5.0  evidence integrity
v0.6.0  investigation storage and API
v0.7.0  Unity forensic replay viewer
v0.8.0  analytics and statistics
v0.9.0  runtime diagnostics and hardening
v1.0.0  final portfolio release
```

Patch releases may be used for real fixes:

```text
v0.3.1  parser boundary bug fix
v0.5.1  evidence verification correction
v1.0.1  README or CI fix after final release
```

## What counts as releasable

A milestone is releasable when:

```text
the milestone scope is complete
open issues for that milestone are closed or explicitly deferred
README does not overclaim
documentation matches implemented behavior
tests pass locally
CI passes remotely once CI exists
working tree is clean
CHANGELOG is updated
tag message is clear
GitHub release notes explain verification and limitations
```

A milestone is not releasable when:

```text
the demo only works on one unrecorded local machine state
README claims features that do not exist
CI is red or meaningless
known broken behavior is hidden
generated files are committed accidentally
security boundaries are unclear
release notes are vague
```

## Scope discipline

A release should have one coherent purpose.

Good release scope:

```text
v0.3.0 - Protocol parser and hardening

Includes:
framed parser
stable parser errors
malformed-input tests
parser fuzz target build

Excludes:
Unity viewer
SQLite investigation
analytics
cloud deployment
```

Bad release scope:

```text
parser, dashboard, AI, Kubernetes, README rewrite, random Unity scene,
Docker changes, and some fixes
```

Mixed releases make review harder and history less useful.

## Pre-release checklist

Run from repository root:

```bash
git status --short
git log --oneline -5
git diff --check
```

Then run the available project checks.

Early `v0.1.0` checks:

```bash
find docs -type f -name '*.md' -print
test -f README.md
test -f SECURITY.md
test -f CHANGELOG.md
test -f docs/architecture.md
test -f docs/threat-model.md
test -f docs/protocol.md
test -f docs/evidence-integrity.md
test -f docs/github-workflow.md
test -f docs/debugging-workflow.md
test -f docs/release-process.md
```

Later checks should include:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m pytest tools/python/tests
docker compose -f infra/docker/compose.yml build
docker compose -f infra/docker/compose.yml run --rm smoke
```

Do not list checks in release notes unless they actually ran.

## Documentation review

Before tagging, review public-facing files:

```text
README.md
SECURITY.md
CHANGELOG.md
docs/architecture.md
docs/threat-model.md
docs/protocol.md
docs/evidence-integrity.md
docs/github-workflow.md
docs/debugging-workflow.md
docs/release-process.md
```

Check for:

```text
claims ahead of implementation
unsafe security language
ambiguous offensive scope
broken command examples
outdated roadmap
inaccurate release status
student-like filler
AI-sounding generic phrasing
```

Documentation should be technical, specific, and grounded in repository state.

## Changelog policy

`CHANGELOG.md` must be grounded in actual Git history.

A changelog entry should describe:

```text
what changed
why it matters
what files or systems were affected
what is intentionally not included
```

Avoid:

```text
invented history
future-tense features
marketing language
huge unsorted lists
claims not visible in the repository
```

Useful history commands:

```bash
git tag --sort=v:refname
git log --oneline --decorate --graph --all
git show --stat --oneline HEAD
```

## Tagging policy

Use annotated tags:

```bash
git tag -a v0.1.0 -m "Blueprint and engineering skeleton"
git push origin v0.1.0
```

Do not move published tags except for an exceptional correction before anyone could reasonably consume the release. Prefer a patch release instead.

Before tagging:

```bash
git status --short
```

Expected output should be empty.

## GitHub release notes

Release notes should include:

```text
release purpose
main changes
verification commands
known limitations
explicit non-scope
```

Example structure:

```text
## Purpose

Defines Tickline's blueprint and engineering skeleton.

## Included

- Project identity
- Architecture documentation
- Threat model
- Protocol specification
- Evidence-integrity specification
- GitHub workflow policy
- Debugging workflow
- Security policy
- Release process

## Verification

Commands or checks that were actually run.

## Limitations

No runtime implementation yet.
No protocol parser yet.
No evidence writer yet.
No Unity viewer yet.
```

The limitations section is important. It prevents the release from pretending to be more complete than it is.

## Issue and milestone closure

Before release:

```bash
gh issue list --milestone "v0.1.0 - Blueprint and engineering skeleton"
```

Each issue should be:

```text
closed because it is complete
or explicitly deferred with a comment explaining why
```

Close the milestone only after the release tag is pushed and the GitHub release exists.

## CI policy

Once CI exists, releases require green CI.

Before CI exists, release notes must not claim CI validation.

After CI exists:

```bash
gh run list
gh run watch --exit-status
```

CI should validate real behavior. A workflow that only prints messages is not a quality gate.

## Artifact policy

Release artifacts may include:

```text
source archive generated by GitHub
sample evidence files when intentionally curated
screenshots when presentation milestone requires them
signed manifests when implemented
```

Do not attach:

```text
local databases
private logs
generated build folders
Unity Library or Temp directories
secrets
machine-specific configuration
unverified binaries
```

Binary releases should not appear until the build and signing story is credible.

## Security review before release

Before tagging security-adjacent work, check:

```text
Does the release target only owned code?
Does README avoid bypass language?
Does SECURITY.md still match the implementation?
Are malformed inputs handled safely?
Are evidence claims accurate?
Are secrets excluded?
Are CI permissions conservative?
Are Docker privileges justified?
```

Security-sensitive releases should not be rushed to preserve the roadmap.

## Hotfix process

Use a hotfix only for a real released problem.

Hotfix steps:

```text
reproduce the bug
identify affected tag
create hotfix branch when useful
fix the smallest real cause
add regression check where practical
update changelog
tag patch release
write release notes explaining impact
```

Example:

```bash
git checkout main
git pull
git checkout -b hotfix/evidence-verifier-chain-gap
```

A typo fix after release may be committed directly to `main` and released later, unless it materially affects public understanding.

## Revert process

For public history, prefer revert:

```bash
git revert <commit>
```

A revert should explain:

```text
which behavior is being reverted
why it was wrong
whether a replacement is planned
```

Do not rewrite public `main` history to hide mistakes.

## Release failure policy

A release attempt can be stopped.

Stop release when:

```text
a check fails
documentation overclaims
the working tree is dirty
a security boundary is unclear
a generated/private file is staged
CI is red
a demo command cannot be reproduced
```

Stopping a release is normal engineering discipline.

## Post-release review

After publishing:

```bash
git tag --sort=v:refname
gh release view v0.1.0
gh issue list --state open
```

Then check the GitHub page in a browser:

```text
README renders correctly
release is visible
topics and description are accurate
issues/milestone state is coherent
no private/generated files are present
```

## Summary

A Tickline release should be boring in the best sense:

```text
accurate
scoped
tested
documented
tagged
reviewable
honest about limitations
```

The project should never trade credibility for the appearance of speed.
