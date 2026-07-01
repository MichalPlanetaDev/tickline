# Security Policy

Tickline is a lawful defensive security-engineering range.

The project exists to study and demonstrate server-authoritative simulation, protocol hardening, evidence integrity, runtime diagnostics, and forensic replay against owned code. It must not be used to attack third-party games, services, anti-cheat products, users, networks, or infrastructure.

## Defensive-use boundary

Allowed use:

```text
testing Tickline-owned protocol parsers
testing Tickline-owned simulation code
testing Tickline-owned validation logic
testing Tickline-owned evidence artifacts
testing local Docker/runtime behavior
testing controlled local clients and fixtures
writing defensive documentation
writing reproducible local tests
reporting vulnerabilities in Tickline itself
```

Not allowed:

```text
cheat development
cheat loaders
bypass tooling
DLL injection against third-party games
kernel evasion
DMA tooling
credential theft
malware
stealth persistence
process hollowing
attacks against live services
attacks against real anti-cheat systems
instructions for evading production security systems
unauthorized reverse engineering of third-party software
```

Tickline must stay inside owned-code defensive research.

## Threat model boundary

Tickline assumes the attacker may control a local client.

The attacker may send:

```text
malformed frames
oversized frames
unsupported protocol versions
valid frames with invalid claims
replayed messages
non-monotonic sequence numbers
manipulated timestamps
impossible movement claims
invalid hit claims
message floods
tampered local evidence files
```

Tickline does not assume permission to attack external systems.

A test is in scope only when:

```text
the target code is part of this repository
the environment is local or explicitly controlled
the behavior can be reproduced safely
the goal is validation, detection, rejection, evidence, or investigation
```

## Reverse-engineering boundary

Tickline may include reverse-engineering literacy only for owned artifacts.

Allowed examples:

```text
inspecting Tickline binaries
reviewing compiler output from Tickline code
studying memory layout of Tickline structures
debugging Tickline crashes
using disassembly to understand Tickline-generated code
documenting safe debugging methodology
```

Not allowed examples:

```text
reverse engineering third-party games
reverse engineering commercial anti-cheat systems
extracting offsets from live games
documenting bypass paths
publishing evasion techniques
turning owned-code experiments into third-party attack guidance
```

The purpose is systems understanding, not evasion.

## Vulnerability reports

Reports are welcome when they concern Tickline itself.

Useful reports include:

```text
parser crash on malformed input
undefined behavior in C++ code
memory-safety issue
unbounded allocation from untrusted input
evidence verifier accepts tampered artifacts
hash-chain verification bug
signature verification bug
CI workflow permission issue
secret exposure
Docker privilege issue
unsafe documentation that could enable misuse
```

Less useful reports:

```text
missing production anti-cheat features
requests to support real games
requests for bypass examples
requests for cheat implementation
requests for kernel evasion
generic vulnerability scans without a reproducible Tickline issue
```

## Reporting method

Use GitHub Security Advisories when available.

If advisories are not enabled, open a GitHub issue only when the report does not expose an exploitable vulnerability or private secret.

For sensitive reports, include:

```text
affected component
repository commit or tag
operating system
compiler version if relevant
exact reproduction steps
expected behavior
actual behavior
minimal input file or command
impact assessment
whether the issue affects only local controlled use
```

Do not include:

```text
real secrets
third-party proprietary data
private user data
live-service details
exploit code targeting external systems
```

## Responsible disclosure expectations

Researchers should:

```text
test only owned Tickline code
avoid disrupting unrelated systems
avoid accessing private data
avoid persistence or lateral movement
avoid destructive payloads
provide minimal reproducible examples
give maintainers reasonable time to respond
```

Tickline maintainers should:

```text
acknowledge valid reports
reproduce the issue
classify severity
fix or document the limitation
add regression tests where practical
credit reporters when appropriate
avoid overstating the fix
```

## Severity guidance

Severity is based on Tickline impact, not production anti-cheat impact.

| Severity | Meaning |
|---|---|
| Low | Documentation ambiguity, minor hardening gap, non-security runtime issue |
| Medium | Reproducible parser or validation bug without memory corruption |
| High | Evidence integrity bypass, unbounded resource use, serious CI/Docker security issue |
| Critical | Memory corruption, arbitrary code execution in Tickline tooling, committed secret, or signature verification failure that makes tampered evidence appear valid |

Tickline is a local range, but security bugs should still be handled seriously.

## Evidence integrity security

Evidence-related bugs are in scope.

Important cases:

```text
record_hash computed incorrectly
previous_hash not verified
record reordering not detected
record deletion not detected
manifest hash mismatch ignored
invalid signature accepted
unsigned artifact treated as signed
SQLite ingest accepts corrupted evidence
Unity viewer displays corrupted evidence as valid
Python analytics ignores verification failure
```

Evidence tooling must distinguish:

```text
valid
valid_unsigned
invalid
unsupported
untrusted
```

It must not silently repair corrupted artifacts.

## Cryptography policy

Tickline may use cryptography for integrity and authenticity of local artifacts.

Allowed:

```text
SHA-256 for hash chaining
Ed25519 or another established signature scheme for manifests
well-maintained cryptographic libraries
test fixture keys clearly marked as non-production
signature verification tests
documentation of limitations
```

Not allowed:

```text
custom cryptographic primitives
homegrown encryption schemes
hardcoded production-like private keys
committed real private keys
claims of production authenticity without key management
security theater without verification tests
```

Private signing keys must not be committed.

## Secret handling

Never commit:

```text
API tokens
GitHub tokens
SSH private keys
cloud credentials
database passwords
private signing keys
production certificates
.env files
real user data
```

Allowed:

```text
sample configuration with fake values
public keys
clearly labeled non-production test fixtures
documentation explaining expected secret locations
```

If a secret is committed accidentally:

```text
revoke the secret immediately
remove it from current files
rotate any affected credentials
document the incident if relevant
do not pretend history rewrite alone fixes exposure
```

## GitHub Actions security

Workflow files are part of Tickline's security boundary.

Required principles:

```text
use least-privilege permissions
avoid unnecessary write permissions
avoid exposing secrets to untrusted code
avoid privileged execution for untrusted pull requests
keep shell commands explicit
avoid curl-pipe-shell installation patterns
pin third-party actions when practical
review workflow changes carefully
```

CI should prove useful engineering properties, not create fake confidence.

## Docker and runtime security

Docker usage should be reproducible and conservative.

Expected properties:

```text
minimal images where practical
non-root runtime user where practical
explicit working directory
explicit mounted artifact paths
no secrets baked into images
clear health or smoke command
clear log output
```

Docker must not hide broken local setup assumptions.

## Safe fuzzing policy

Fuzzing is allowed only against owned Tickline targets.

Allowed fuzzing targets:

```text
protocol parser
evidence parser
manifest verifier
replay artifact loader
```

Fuzz targets should:

```text
accept byte buffers
avoid network access
avoid external services
avoid writing persistent artifacts
avoid global mutable state
run deterministically where practical
be compatible with sanitizers
```

Fuzzing must not target third-party binaries or services.

## Documentation safety

Documentation must not provide operational offensive guidance.

Acceptable documentation:

```text
threat models
defensive validation logic
parser hardening notes
owned-code debugging notes
safe reproduction commands
evidence verification behavior
false-positive analysis
```

Unacceptable documentation:

```text
bypass recipes
cheat implementation steps
live-service attack paths
third-party game memory offsets
anti-cheat evasion procedures
credential theft methods
malware persistence guidance
```

When in doubt, keep the writing defensive, bounded, and tied to Tickline-owned code.

## Supported versions

Tickline is currently pre-`v1.0.0`.

Security fixes should target:

```text
main
latest tagged release when applicable
```

Before `v1.0.0`, breaking changes may happen when they improve safety, correctness, or architecture. Such changes must be documented in `CHANGELOG.md`.

## Maintainer checklist

Before merging security-sensitive work:

```text
Is the target owned by Tickline?
Does this change preserve the defensive boundary?
Are trust boundaries documented?
Are malformed inputs tested?
Are resource limits explicit?
Are errors classified?
Does evidence explain decisions?
Does CI test the relevant path?
Are secrets excluded?
Does documentation avoid misuse guidance?
```

If the answer is unclear, keep the work in design or research state until the boundary is clear.

## Summary

Tickline is safe only if its scope remains precise.

```text
owned code
controlled clients
explicit trust boundaries
authoritative validation
tamper-evident evidence
human investigation
reproducible local runtime
no third-party targeting
no bypass tooling
```

Security work in this repository must strengthen that chain.
