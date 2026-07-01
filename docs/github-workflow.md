# GitHub Workflow

The repository should show that work is scoped, reviewed, tested, released, and explainable. It should not imitate a large-team process where that process does not add value.

## Principle

The rule is simple:

```text
use process when it protects engineering quality
do not use process as theatre
```

A professional workflow is not measured by how many branches, pull requests, labels, or ceremonies exist. It is measured by whether the history helps another engineer understand what changed, why it changed, how it was verified, and how to recover if it breaks.

## Repository roles

Tickline uses GitHub for:

```text
source control
issue tracking
milestone planning
release history
CI visibility
engineering notes
security boundaries
review context for risky changes
```

GitHub is not used for:

```text
performative pull requests
fake collaboration
empty labels
branch churn with no review value
inflated release notes
hiding unstable work behind vague commits
```

## Main branch policy

`main` is the public stable branch.

Allowed direct commits to `main`:

```text
small documentation corrections
single-file specification updates
typo fixes
small README updates
changelog edits
low-risk repository metadata changes
small issue-template changes
```

Direct commits must still be clean:

```text
working tree reviewed before commit
commit message describes the logical change
no unrelated files included
no generated junk committed
no secrets committed
CI must remain green once CI exists
```

Direct commits are not an amateur shortcut when used correctly. They are appropriate for low-risk solo work where a pull request would add ceremony but no real review value.

## Branch policy

Branches are used for meaningful engineering units.

Use a branch for:

```text
C++ simulation core
protocol parser implementation
evidence hash-chain implementation
CI workflow changes
Docker runtime changes
Unity viewer importer
Python analytics module
fuzzing and sanitizer hardening
cross-cutting architecture changes
release candidates
```

Do not create a branch for:

```text
one typo
one paragraph
one small Markdown correction
one obvious metadata fix
```

A branch should represent work that deserves isolation, CI validation, or review context.

## Branch naming

Preferred branch names:

```text
feature/cpp-simulation-core
feature/protocol-parser
feature/evidence-hash-chain
feature/unity-replay-importer
feature/python-session-report

hardening/parser-fuzz-target
hardening/asan-ubsan-ci
hardening/github-actions-permissions

infra/docker-smoke-runtime
infra/cmake-presets
infra/ci-quality-gates

docs/threat-model-revision
docs/release-process
docs/debugging-workflow

release/v0.2.0
hotfix/evidence-verifier-crash
```

Avoid names such as:

```text
update
changes
fix
new-stuff
final
final-final
test
```

## Pull request policy

A pull request should exist when the change benefits from review context.

Use a PR for:

```text
risky implementation work
multi-file changes
CI changes
security-sensitive changes
protocol changes
parser changes
evidence-integrity changes
release candidates
changes that need discussion before merge
changes that should preserve reasoning publicly
```

A PR should explain:

```text
problem
approach
trust boundary affected
tests or checks performed
known limitations
issue closed
```

A PR should not exist merely to create the appearance of professional workflow.

## Issue policy

Issues are used for scoped work, not vague ambitions.

Good issue:

```text
Title:
Add framed protocol parser error taxonomy

Body:
Implement parser result types for invalid magic, unsupported version,
invalid header size, frame too small, frame too large, unknown message type,
payload too short, and payload too long.

Acceptance criteria:
- Parser has stable error enum.
- Tests cover each error.
- No network code is required to test parser.
- Documentation stays synchronized with docs/protocol.md.
```

Bad issue:

```text
Make protocol better
Add security stuff
Improve project
Do advanced things
```

An issue should be small enough to finish, but large enough to matter.

## Milestone policy

Milestones group coherent release work.

A milestone should have:

```text
clear version
specific engineering goal
closed issue list
acceptance criteria
release notes
tag after completion
```

A milestone should not mix unrelated work.

Example:

```text
v0.3.0 - Protocol boundary

Scope:
framed parser
stable parser errors
golden frame fixtures
malformed-input tests
fuzz target build

Non-scope:
Unity viewer
SQLite investigation
machine learning
cloud deployment
```

## Label policy

Labels are used to filter and understand work.

Area labels:

```text
area:architecture
area:simulation
area:protocol
area:security
area:evidence
area:unity
area:analytics
area:devops
```

Kind labels:

```text
kind:feature
kind:bug
kind:docs
kind:hardening
kind:research
```

Priority/status labels:

```text
priority:high
status:blocked
```

Labels should remain useful. If a label does not help triage work, it should not exist.

## Commit policy

A commit should represent one logical change.

Good commit messages:

```text
docs: define GitHub workflow policy
build: add CMake presets
ci: add C++ build and test workflow
protocol: add framed parser result type
protocol: test malformed frame boundaries
simulation: add deterministic tick clock
evidence: add canonical record hash calculation
infra: add Docker smoke runtime
unity: document replay viewer import boundary
```

Bad commit messages:

```text
update
fix
stuff
changes
work
final
please work
```

Use lowercase scope prefixes when they add clarity:

```text
docs:
build:
ci:
protocol:
simulation:
evidence:
infra:
unity:
python:
security:
```

The prefix is not mandatory. Clarity is mandatory.

## Commit contents

Before committing, check:

```bash
git status --short
git diff
git diff --check
```

The commit should not include:

```text
editor files
temporary logs
generated build folders
Unity Library or Temp
local databases
local evidence artifacts unless intentionally added as samples
secrets
private keys
machine-specific configuration
```

## Rebase policy

Rebase is used to clean local history before publishing or to update a feature branch.

Allowed:

```text
interactive rebase on unpublished local commits
rebase feature branch onto current main before PR
squash small noisy local commits into logical commits
```

Avoid:

```text
rewriting public main history
rewriting tags
rewriting history after others may have based work on it
using rebase to hide mistakes instead of documenting a meaningful correction
```

Professional history is clean, but not fake.

## Merge policy

For solo Tickline work:

```text
direct commit is acceptable for small low-risk changes
squash merge is acceptable for feature branches with noisy commits
merge commits are acceptable only when preserving branch history matters
```

Do not squash away important review context if intermediate commits document meaningful steps.

## Cherry-pick policy

Cherry-pick is used for selective transfer of a known commit.

Appropriate cases:

```text
backport small hotfix
move a documentation fix from one branch to another
recover one good commit from an abandoned branch
```

Inappropriate cases:

```text
randomly copying work between unclear branches
building release history from unreviewed fragments
avoiding proper branch cleanup
```

## Bisect policy

`git bisect` is used when a regression exists and the bad commit is not obvious.

Good use case:

```text
parser accepted oversized frames in current main
older tag v0.3.0 rejected them correctly
bisect identifies the commit that broke frame length validation
```

A project that can be bisected has useful tests and meaningful commits.

## Revert policy

Use revert when a public commit must be undone.

Prefer:

```bash
git revert <commit>
```

over rewriting public history.

A revert commit should explain why the change is being backed out.

## Hotfix policy

Use hotfix branches only for released versions or urgent main breakage.

Example:

```text
hotfix/evidence-verifier-crash
```

Hotfix flow:

```text
identify failing behavior
write or reproduce test
fix minimal cause
run checks
merge
tag patch release if needed
document in changelog
```

## Release policy

A release is not just a tag.

A release requires:

```text
clean working tree
green CI
updated changelog
README synchronized with actual capability
version tag
GitHub release notes
known limitations documented
```

Use annotated tags:

```bash
git tag -a v0.1.0 -m "Blueprint and engineering skeleton"
git push origin v0.1.0
```

Release notes should describe:

```text
what changed
why it matters
how to verify it
what is intentionally out of scope
```

## Branch protection

Branch protection should be enabled when CI has meaningful checks.

Before CI exists:

```text
do not pretend branch protection proves quality
avoid force-push to main
avoid rewriting public main history
```

After CI exists:

```text
require status checks before merge
block force-push to main
require linear history if it improves readability
require PR only for risky or protected areas
```

Potential protected areas later:

```text
.github/workflows/
cpp/protocol/
cpp/evidence/
infra/
SECURITY.md
```

## CI expectations

CI should check real project risks.

Initial useful gates:

```text
CMake configure
C++ build
C++ tests
documentation sanity checks
git diff whitespace check
```

Later gates:

```text
clang-format
clang-tidy
AddressSanitizer
UndefinedBehaviorSanitizer
parser fuzz target build
Python tests
Docker build
Docker smoke test
dependency/security checks
```

CI should not be inflated with jobs that do not test anything meaningful.

## GitHub Actions security

Workflow files are part of the security boundary.

Rules:

```text
use least-privilege permissions
avoid broad write permissions
do not expose secrets to untrusted code
do not run privileged workflows on untrusted pull-request code
prefer pinned third-party actions when practical
keep shell scripts explicit and reviewable
avoid curl-pipe-shell installation patterns in CI
```

Tickline should eventually document any exception.

## Secrets policy

Never commit:

```text
API keys
tokens
private signing keys
cloud credentials
database passwords
SSH private keys
production-like certificates
.env files
```

Allowed:

```text
clearly labeled test fixtures
public keys
sample config with fake values
documentation explaining where real secrets would live
```

If evidence signing is implemented, test keys must be obviously non-production.

## Issue-to-commit relationship

Not every commit needs an issue.

Require an issue for:

```text
milestone work
feature work
security-sensitive work
bug fixes with investigation value
protocol changes
CI changes
release changes
```

No issue required for:

```text
typo
formatting correction
small wording fix
minor README synchronization
```

The repository should not become bureaucratic.

## Solo review checklist

Before pushing:

```text
Does this change match one clear purpose?
Does the commit include unrelated files?
Does documentation overclaim capability?
Does this add process without value?
Does this increase security misuse risk?
Can another engineer understand the change?
Can this be tested now or soon?
```

If the answer is poor, revise before pushing.

## Anti-patterns

Avoid:

```text
branch per paragraph
PR per typo
large commits with unrelated files
labels that are never used
issues that describe wishes instead of work
releases that do not correspond to real state
README claims ahead of implementation
CI that only prints messages
Docker that hides broken local setup
security documentation that permits ambiguous misuse
```

These make a repository look less professional, not more.

## What professional GitHub usage should prove

Tickline's GitHub history should prove:

```text
the project has direction
changes are intentional
riskier work is isolated
tests become stricter over time
releases correspond to real capability
documentation matches implementation
security boundaries are preserved
mistakes are corrected visibly
```
