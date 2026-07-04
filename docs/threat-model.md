# Threat Model

Tickline is a lawful defensive security-engineering range. The threat model exists to keep the project technically serious, reviewable, and safe.

The central assumption is simple:

```text
the client is untrusted
```

Every protocol message, timestamp, sequence number, position, velocity, hit claim, and target reference received from a client must be treated as a claim. A claim becomes trusted only after it crosses a defined boundary and is validated against authoritative server state.

## Scope

Tickline models adversarial behavior against owned code inside this repository.

Allowed targets:

```text
Tickline protocol parser
Tickline deterministic simulation
Tickline validation rules
Tickline controlled clients
Tickline local evidence artifacts
Tickline local investigation tooling
Tickline local Docker/runtime environment
```

Allowed testing methods:

```text
malformed protocol frames
invalid client claims
replayed local messages
sequence abuse
timestamp abuse
rate abuse
invalid movement claims
invalid hit claims
parser fuzzing against owned code
tamper tests against local evidence artifacts
controlled Docker/runtime failure tests
```

The project must remain defensive. The goal is to validate, reject, record, explain, and investigate suspicious behavior in a controlled range.

## Explicit non-scope

Tickline must not include:

```text
cheat loaders
third-party game targeting
real anti-cheat bypasses
DLL injection against third-party games
kernel evasion
DMA tooling
credential theft
malware
stealth persistence
process hollowing
remote exploitation
production service attacks
instructions for evading real anti-cheat systems
instructions for attacking live services
```

Reverse-engineering literacy may be documented only in safe, owned-code contexts. For example, examining symbols, memory layout, compiler output, or protocol behavior of Tickline binaries is acceptable. Applying those techniques to third-party games or commercial anti-cheat systems is outside scope.

## Assets

Tickline protects these assets:

| Asset | Why it matters |
|---|---|
| Authoritative simulation state | The server-side state is the source of truth |
| Protocol boundary | Malformed input must not corrupt state or crash the system |
| Validation decisions | Findings must be explainable and reproducible |
| Evidence records | Telemetry must preserve what happened and why it mattered |
| Evidence integrity chain | Investigation artifacts must expose tampering |
| Rule definitions | Detection logic must be reviewable and versioned |
| Investigation database | Stored evidence must remain queryable and consistent |
| Runtime logs | Operators need enough context to debug failures |
| CI results | Builds and tests must prove the project remains reproducible |
| Documentation | Reviewers need clear boundaries, assumptions, and limitations |

Tickline does not attempt to protect a commercial production game economy, player accounts, payment data, private user data, or live-service infrastructure.

## Trust boundaries

### Boundary 1: Client to protocol parser

All client input is untrusted.

Untrusted fields include:

```text
frame length
message type
protocol version
client timestamp
sequence number
session identifier
entity identifier
position
velocity
acceleration
aim direction
target identifier
claimed hit distance
claimed latency
```

Required controls:

```text
bounded frame reads
maximum frame size
protocol version validation
strict parser error handling
malformed input classification
no unchecked integer conversions
no unbounded allocation from client-controlled lengths
no undefined behavior on invalid input
```

The parser should not normalize hostile data into apparently valid data. Rejection must be explicit.

Implemented protocol controls include a fixed big-endian header, exact frame and
payload sizes, a configurable maximum frame size, stable parser errors, strict
one-frame parsing, bounded incremental stream reassembly, persistent fail-closed
stream state, compatibility tests, and parser fuzzing against a committed
regression corpus. These controls do not provide transport authentication,
confidentiality, rate limiting, or connection-level denial-of-service handling.

### Boundary 2: Protocol parser to simulation core

A syntactically valid message is still not trusted.

The parser can prove that bytes matched a message format. It cannot prove that a claim is physically or temporally valid.

Required controls:

```text
server-owned session state
server-owned tick clock
sequence monotonicity checks
movement envelope checks
cooldown checks
target existence checks
hit geometry checks
server-side tolerance windows
state-transition validation
```

### Boundary 3: Simulation core to evidence pipeline

A validation result must become evidence without losing meaning.

Required preserved context:

```text
rule identifier
severity
entity or session identifier
server timestamp
client sequence when present
observed value
expected limit
decision
human-readable explanation
machine-readable code
```

The evidence pipeline must not collapse distinct findings into vague labels. A reviewer should be able to understand why the event was suspicious.

### Boundary 4: Evidence artifact to investigation tooling

Investigation tooling consumes artifacts that may be incomplete or tampered with.

Required controls:

```text
schema version checks
required-field validation
hash-chain verification
manifest verification when signing is implemented
clear unsupported-version behavior
corruption reporting
no silent repair of broken evidence
```

### Boundary 5: Local runtime to operator environment

Tickline runs locally, in WSL/Linux and Docker. Runtime behavior should be reproducible and inspectable.

Required controls:

```text
documented commands
non-root containers where practical
explicit volume mounts
clear log paths
graceful shutdown behavior
health checks where useful
no secrets committed to the repository
no dependence on private local machine state
```

## Attacker model

Tickline models an attacker who controls one or more clients.

The attacker can:

```text
send malformed frames
send oversized frames
send unsupported protocol versions
send valid frames with invalid claims
replay old messages
skip or repeat sequence numbers
manipulate client timestamps
claim impossible movement
claim impossible hit geometry
claim non-existent targets
send messages too quickly
disconnect and reconnect repeatedly
attempt to poison telemetry with malformed values
tamper with local evidence files after generation
```

The attacker cannot initially:

```text
write arbitrary memory inside the server process
modify the server binary
modify CI results
modify Git history
steal signing keys
gain root on the host
control the database engine internals
control the compiler toolchain
```

If a later milestone adds local tamper or supply-chain simulation, that must be documented as a separate attacker capability. Attacker powers must not be silently mixed.

## Abuse cases

### Malformed protocol input

Example behavior:

```text
client sends invalid framing
client sends unknown message type
client sends unsupported protocol version
client sends truncated payload
client sends frame larger than configured limit
```

Expected response:

```text
reject the input
classify parser error
avoid undefined behavior
avoid process crash
record protocol finding when appropriate
close or throttle connection depending on policy
```

### Replay attempt

Example behavior:

```text
client resends an old valid command
client repeats a previous sequence number
client reuses stale timestamp data
```

Expected response:

```text
detect non-monotonic sequence
detect stale timing where applicable
reject or flag the command
record replay-relevant evidence
```

### Impossible movement claim

Example behavior:

```text
client claims position beyond allowed movement envelope
client claims inconsistent velocity
client compresses too much movement into too little server time
```

Expected response:

```text
compare against authoritative state
apply configured tolerance
reject, clamp, or flag according to validation policy
record observed distance and expected limit
```

### Invalid hit claim

Example behavior:

```text
client claims a target that does not exist
client claims a hit outside maximum range
client claims a hit inconsistent with aim direction
client claims an impossible hit distance
```

Expected response:

```text
verify target existence
verify ray or swept geometry
verify distance and tolerance
reject or flag invalid claim
record hit-validation evidence
```

### Rate abuse

Example behavior:

```text
client floods commands
client sends bursts beyond configured message rate
client attempts to exhaust parser or validation resources
```

Expected response:

```text
enforce rate limits
bound processing cost
record rate-limit finding
close abusive connection when policy requires
```

### Evidence tampering

Example behavior:

```text
operator or local process edits evidence file
record is deleted
record is reordered
record content is modified
manifest is missing
```

Expected response:

```text
detect hash-chain mismatch
report exact verification failure
avoid treating corrupted evidence as valid
preserve clear investigation output
```

## Defensive controls by layer

| Layer | Primary controls |
|---|---|
| Protocol | Framing limits, version checks, parser error codes, malformed input tests |
| Simulation | Server-owned state, fixed ticks, deterministic validation, tolerance windows |
| Evidence | Structured records, rule IDs, hash chaining, manifest verification |
| Storage | Schema versioning, required fields, integrity checks before ingest |
| Analytics | Descriptive statistics, outlier explanation, false-positive notes |
| Unity viewer | Read-only artifact loading, replay visualization, evidence status display |
| Linux/runtime | Logs, signals, permissions, process inspection, container boundaries |
| CI/CD | Build/test gates, sanitizers, formatting checks, Docker smoke test |
| GitHub | Issues, milestones, reviewable branches, release notes, changelog |

## False-positive policy

A suspicious finding is not an enforcement decision.

Tickline must distinguish:

```text
detection
evidence
investigation
enforcement
```

This project implements detection and evidence. It may support investigation. It must not claim production-grade automatic enforcement.

False-positive-aware design requires:

```text
explicit tolerance values
recorded observed values
recorded expected limits
stable rule identifiers
human-readable explanations
known limitations
test cases for borderline behavior
separation between low, medium, and high severity
```

Examples of legitimate causes for suspicious-looking behavior in real systems:

```text
network jitter
packet loss
clock drift
client frame spikes
server tick delay
floating-point tolerance error
interpolation mismatch
configuration mistakes
test fixture bugs
```

Tickline should discuss these risks instead of pretending every finding proves cheating.

## Evidence requirements

Evidence records should be:

```text
structured
versioned
ordered
tamper-evident
machine-readable
human-reviewable
stable across runs when inputs are deterministic
```

Evidence should include enough context to support review:

```text
session id
entity id
rule id
severity
server timestamp
client sequence if present
observed value
expected value
decision
explanation
previous hash
record hash
```

The exact schema will be defined in `docs/evidence-integrity.md`.

## Cryptographic boundaries

Tickline may use cryptography for integrity and authenticity of local artifacts.

Appropriate uses:

```text
hash-chained evidence records
signed evidence manifests
signature verification in tooling
tamper-detection tests
key-handling documentation
```

Inappropriate uses:

```text
custom cryptographic primitives
homegrown encryption schemes
claims of production trust without key-management design
hardcoded production-like secrets
security theater without verification tests
```

If signatures are implemented, private keys must not be committed. Test keys may exist only when clearly labeled as non-production fixtures.

## Supply-chain and CI threat notes

Tickline should eventually account for build and dependency integrity.

Relevant risks:

```text
compromised dependency
unreviewed generated artifact
modified Docker base image
CI workflow drift
unchecked binary artifact
missing reproducibility command
```

Possible later controls:

```text
dependency pinning
SBOM generation
container scanning
artifact checksums
signed release artifacts
minimal Docker images
least-privilege CI permissions
```

These controls should be introduced only when they are testable and relevant to the local range.

## Runtime and operational risks

Runtime failures that should be diagnosable:

```text
server fails to bind port
client cannot connect
process exits on malformed input
evidence directory lacks permissions
disk fills during telemetry write
container runs as wrong user
database ingest fails
hash-chain verification fails
CI passes locally but fails in GitHub Actions
```

The debugging workflow should document how to inspect these failures using Linux and project-specific tools.

## Security review checklist

Before merging security-sensitive work, verify:

```text
Does this increase offensive misuse risk?
Does it target only owned code?
Are trust boundaries documented?
Are malformed inputs tested?
Are limits explicit?
Are errors classified?
Can evidence explain the decision?
Can the behavior be reproduced?
Does CI test the relevant path?
Does documentation describe limitations?
```

If the answer is unclear, the change should remain in research or design state before implementation.

## Non-goals

Tickline does not try to prove:

```text
production anti-cheat completeness
commercial fraud prevention coverage
kernel-level detection
third-party game reverse engineering
real-player enforcement
cloud-scale ingestion
global infrastructure resilience
automatic cheat attribution
```

The project is narrower and more credible than that. It proves a controlled chain of security engineering decisions inside owned code.

## Summary

Tickline's threat model is built around controlled distrust.

```text
untrusted client
explicit protocol boundary
authoritative validation
structured evidence
tamper detection
human investigation
reproducible runtime
safe defensive scope
```

Every future feature should strengthen that chain. If it does not, it does not belong in the project.
