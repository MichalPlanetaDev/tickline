# Evidence Integrity Specification

Tickline treats evidence as a first-class engineering artifact. A validation finding is only useful when it can be reviewed, replayed, and checked for tampering.

This document defines the implemented `v0.4.0` evidence record, SHA-256 chain, binary archive, trusted-head verification, and deterministic replay model. Digital signatures and key management remain future work.

## Design goals

Evidence in Tickline should be:

```text
structured
versioned
ordered
tamper-evident
machine-readable
human-reviewable
deterministic when inputs are deterministic
portable across local tools
clear about limitations
```

Evidence must answer:

```text
what happened
when the server observed it
which session/entity it involved
which rule evaluated it
what value was observed
what limit or expectation was violated
what decision was made
whether the record chain is intact
```

The evidence system is not designed to prove legal guilt, identify a real user, or support automatic punishment. It is designed to support defensive investigation of controlled local scenarios.

## Evidence pipeline

The intended pipeline is:

```text
authoritative validation result
        ↓
structured telemetry event
        ↓
canonical evidence record
        ↓
hash-chain link
        ↓
evidence artifact
        ↓
manifest
        ↓
verification tooling
        ↓
SQLite ingest / Python analysis / Unity replay viewer
```

Each stage should preserve meaning. The system must not reduce detailed validation context into vague labels.

## Evidence terminology

| Term | Meaning |
|---|---|
| Telemetry event | Raw structured record emitted by runtime behavior |
| Validation finding | Rule result indicating suspicious, invalid, rejected, or corrected behavior |
| Evidence record | Canonical record included in the tamper-evident chain |
| Evidence artifact | File containing ordered evidence records |
| Manifest | Metadata describing an artifact, including hash and optional signature |
| Chain hash | Hash linking one evidence record to the previous record |
| Verification | Process that checks schema, ordering, hashes, and optionally signatures |

## Evidence artifact format

Initial artifact target:

```text
JSON Lines
```

Extension:

```text
.jsonl
```

One line represents one evidence record.

Reasons:

```text
append-friendly
human-inspectable
streamable
works well with CLI tools
easy to ingest into SQLite
easy to process from Python
clear failure location by line number
```

Later exports may include:

```text
JSON summary
CSV report
SQLite database
Unity replay package
signed manifest
```

The JSONL file remains the canonical local evidence stream unless a later release explicitly changes that.

## Canonical record requirements

Every evidence record must contain:

```text
schema_version
record_index
record_type
session_id
server_time_ms
severity
source
decision
previous_hash
record_hash
```

Most validation records should also contain:

```text
entity_id
sequence
client_time_ms
rule_id
observed
expected
explanation
```

Required top-level fields:

| Field | Type | Meaning |
|---|---|---|
| `schema_version` | integer | Evidence schema version |
| `record_index` | integer | Zero-based ordered record index |
| `record_type` | string | Category of evidence record |
| `session_id` | string | Local session identifier |
| `entity_id` | string or null | Entity involved, if applicable |
| `sequence` | integer or null | Client sequence, if available |
| `server_time_ms` | integer | Server-observed time |
| `client_time_ms` | integer or null | Client-claimed time, if relevant |
| `severity` | string | Finding severity |
| `source` | string | Producing subsystem |
| `rule_id` | string or null | Validation or parser rule |
| `observed` | object | Observed values |
| `expected` | object | Expected limits or policy |
| `decision` | string | System decision |
| `explanation` | string | Human-readable summary |
| `previous_hash` | string | Hash of previous record or genesis marker |
| `record_hash` | string | Hash of canonical record content |

Optional fields:

```text
connection_id
remote_endpoint
message_type
parser_error
protocol_version
artifact_id
build_id
policy_id
trace_id
```

Optional fields must be documented before they become part of a released schema.

## Record types

Initial record types:

```text
protocol_event
validation_finding
state_correction
runtime_event
integrity_event
operator_event
```

Meanings:

| Record type | Meaning |
|---|---|
| `protocol_event` | Parser, framing, versioning, or message-boundary event |
| `validation_finding` | Simulation or gameplay claim failed validation |
| `state_correction` | Server corrected or rejected client-visible state |
| `runtime_event` | Runtime lifecycle, startup, shutdown, configuration, or health event |
| `integrity_event` | Verification, hash-chain, manifest, or tamper-detection result |
| `operator_event` | Explicit local operator action, when needed for investigation context |

The early implementation may start with fewer types, but the schema should not make later expansion impossible.

## Severity levels

Severity levels:

```text
info
low
medium
high
critical
```

Guidance:

| Severity | Intended meaning |
|---|---|
| `info` | Normal lifecycle or context event |
| `low` | Suspicious but weak signal |
| `medium` | Clear invalid claim or repeated suspicious behavior |
| `high` | Strong invalid behavior with direct security relevance |
| `critical` | Integrity failure, severe abuse, or condition requiring immediate operator attention |

Severity does not equal enforcement. A `critical` local evidence finding still requires interpretation in context.

## Decisions

Initial decision values:

```text
accepted
rejected
corrected
flagged
disconnected
rate_limited
verification_failed
verification_passed
```

Examples:

```text
valid movement claim -> accepted
impossible movement claim -> rejected
minor prediction mismatch -> corrected
borderline suspicious timing -> flagged
malformed oversized frame -> disconnected
message flood -> rate_limited
hash-chain mismatch -> verification_failed
clean artifact check -> verification_passed
```

A decision should describe what the system did, not what the client "is".

Avoid:

```text
cheater
banned
guilty
malicious_user
```

Use evidence-based language.

## Rule identifiers

Rule IDs must be stable, lowercase, and machine-readable.

Examples:

```text
protocol.invalid_magic
protocol.frame_too_large
protocol.unsupported_version
protocol.payload_too_short
session.sequence_replay
session.sequence_regression
time.client_time_regression
time.client_time_jump
movement.envelope_violation
movement.velocity_violation
state.invalid_transition
combat.fire_cooldown_violation
combat.unknown_target
combat.invalid_aim_vector
combat.hit_distance_violation
combat.hit_geometry_violation
runtime.rate_limit_violation
integrity.hash_chain_mismatch
integrity.manifest_signature_invalid
```

Rule IDs should not be renamed after release without migration notes.

## Observed and expected values

The `observed` field records what the system saw.

Example:

```json
{
  "claimed_delta_mm": 4200,
  "elapsed_server_ms": 100,
  "claimed_velocity_mm_per_s": 42000
}
```

The `expected` field records the policy or limit used for comparison.

Example:

```json
{
  "max_delta_mm": 900,
  "movement_tolerance_mm": 100,
  "policy_id": "movement.v1"
}
```

A useful evidence record should preserve enough numbers to let a reviewer understand the finding without reading the code first.

## Canonicalization

Hashing requires stable canonical representation.

Tickline should define canonical evidence content as:

```text
UTF-8 JSON
object keys sorted lexicographically
no insignificant whitespace
integer values represented as JSON integers
strings normalized as emitted by the writer
record_hash excluded while computing record_hash
previous_hash included while computing record_hash
```

Floating-point values should be avoided in canonical evidence where possible. Prefer integer units:

```text
milliseconds
millimeters
microunits
basis points
integer counters
```

If floating-point values become unavoidable, the schema must specify formatting precision and rounding rules before those values are included in hash calculations.

## Hash algorithm

Initial hash algorithm:

```text
SHA-256
```

Hash string format:

```text
lowercase hexadecimal
```

Genesis previous hash:

```text
0000000000000000000000000000000000000000000000000000000000000000
```

For record `n`:

```text
record_hash[n] = SHA256(canonical_record_without_record_hash[n])
```

The canonical record includes:

```text
previous_hash = record_hash[n - 1]
```

For the first record:

```text
previous_hash = genesis hash
```

The chain is valid only when:

```text
record_index starts at 0
record_index increments by 1
previous_hash for record 0 equals genesis hash
previous_hash for record n equals record_hash of record n - 1
record_hash for every record matches canonical content
```

## Manifest

Every evidence artifact should eventually have a manifest.

Suggested manifest file:

```text
artifact-name.manifest.json
```

Required manifest fields:

```text
manifest_schema_version
artifact_id
artifact_path
artifact_sha256
record_count
first_record_hash
last_record_hash
evidence_schema_version
created_at_utc
producer
build_id
git_commit
hash_algorithm
signature_algorithm
signature
public_key_id
```

Fields may be null before signing is implemented, but the manifest schema should distinguish "not implemented" from "verification failed".

Example unsigned manifest status:

```json
{
  "signature_algorithm": null,
  "signature": null,
  "public_key_id": null
}
```

## Signing policy

Artifact signing is a planned capability, not a `v0.1.0` implementation claim.

Acceptable signing approach:

```text
Ed25519 signatures over the manifest content
or another well-established signature mechanism with clear library support
```

Signing rules:

```text
do not invent custom signature algorithms
do not commit private production-like keys
test keys must be clearly labeled as test fixtures
signature verification must be tested
unsigned artifacts must be reported as unsigned, not invalid
invalid signatures must fail verification loudly
```

The private signing key is outside the evidence artifact. The public verification key may be included as a test fixture or documented local key.

## Key handling

Initial project scope should avoid pretending to solve production key management.

Allowed:

```text
local test key pair for deterministic tests
clearly labeled non-production fixtures
documentation of where production key management would differ
verification tests using fixture keys
```

Not allowed:

```text
committed real private keys
hardcoded production-like secrets
claims of production authenticity without a key-management model
ambiguous key provenance
```

If supply-chain signing or container signing is added later, it should be documented separately from evidence-record signing.

## Verification algorithm

Evidence verification should check:

```text
file exists
file is readable
each line is valid JSON
schema_version is supported
required fields are present
record_index is continuous
previous_hash is correct
record_hash is correct
manifest artifact hash matches file
manifest record_count matches file
manifest first_record_hash matches first record
manifest last_record_hash matches last record
signature is valid when present
```

Verification should produce structured results.

Possible verification statuses:

```text
valid
valid_unsigned
invalid_json
unsupported_schema
missing_required_field
record_index_gap
previous_hash_mismatch
record_hash_mismatch
manifest_hash_mismatch
manifest_record_count_mismatch
signature_missing
signature_invalid
signature_untrusted
```

A corrupted artifact must not be silently repaired by the verifier.

## Tamper cases

Verification tests should cover:

```text
record content changed
record deleted
record inserted
record reordered
previous_hash changed
record_hash changed
manifest artifact hash changed
manifest record count changed
signature missing
signature invalid
unsupported schema version
truncated final line
invalid JSON line
```

A good integrity system makes tampering easy to detect and easy to explain.

## Evidence schema example

Example validation finding:

```json
{
  "schema_version": 1,
  "record_index": 42,
  "record_type": "validation_finding",
  "session_id": "session-0001",
  "entity_id": "player-7",
  "sequence": 118,
  "server_time_ms": 50320,
  "client_time_ms": 50110,
  "severity": "high",
  "source": "simulation",
  "rule_id": "movement.envelope_violation",
  "observed": {
    "claimed_delta_mm": 4200,
    "elapsed_server_ms": 100
  },
  "expected": {
    "max_delta_mm": 900,
    "movement_tolerance_mm": 100,
    "policy_id": "movement.v1"
  },
  "decision": "rejected",
  "explanation": "Client claimed movement exceeded the authoritative movement envelope.",
  "previous_hash": "18b7f5d2a8726f13b8a1c2c7011df4df7f79a7bd5d82b5ec93d2473df6e6b69c",
  "record_hash": "0c6d91b55e50a48183c9fb7103a4b2ecbe89774750c67f04383cf7d3d9ac81f4"
}
```

The hashes above are illustrative. Implementation tests must compute real hashes from canonical content.

## Evidence limitations

Evidence records do not prove everything.

Known limitations:

```text
server-side evidence depends on server configuration
bad tolerance values can create bad findings
local test data does not represent production population behavior
hash chains detect modification after writing, not false data before writing
unsigned artifacts provide tamper evidence but not producer authenticity
client timestamps are claims, not truth
network timing can create ambiguous findings in real systems
```

Documentation and tooling should present evidence as investigation material, not absolute certainty.

## False-positive awareness

Evidence must support review of borderline cases.

A finding should include:

```text
observed values
expected thresholds
policy identifiers
server timing context
client timing claim when present
rule severity
explanation
```

Examples of real-world ambiguity:

```text
jitter
packet loss
server overload
clock drift
prediction error
floating-point mismatch
configuration mistakes
test fixture defects
```

Tickline should not hide these ambiguities. Professional security tooling must make uncertainty visible.

## Storage and ingest behavior

Before ingesting evidence into SQLite, the ingest tool should:

```text
verify schema version
verify required fields
verify hash chain
verify manifest when provided
reject or quarantine invalid artifacts
report exact failure location
```

The database should store enough data to preserve reviewability:

```text
artifact id
record index
record hash
previous hash
session id
entity id
rule id
severity
decision
server time
observed JSON
expected JSON
explanation
```

The database is an investigation index, not the canonical evidence source unless a future release explicitly changes that.

## Unity viewer behavior

The Unity forensic viewer should display evidence integrity status.

Viewer requirements:

```text
show artifact verification status
show chain status
show unsigned versus signed artifact state
show validation findings on timeline
show observed and expected values
avoid mutating evidence artifacts
```

The viewer is a read-only investigation surface.

## Python analytics behavior

Analytics tools should preserve evidence context.

Allowed:

```text
summarize rule frequencies
compute severity distribution
compute outlier scores
compare sessions
generate reports
flag unusual patterns for review
```

Not allowed:

```text
hide rule context behind unexplained scores
claim automatic cheat attribution
modify canonical evidence during analysis
treat unsigned artifacts as signed
ignore failed verification
```

Analytics output should reference source artifact IDs and record hashes where practical.

## CI requirements

Evidence-integrity implementation should eventually be tested in CI.

Expected tests:

```text
canonicalization stable output
hash calculation exact match
valid chain verification
modified content detection
deleted record detection
reordered record detection
manifest mismatch detection
unsigned manifest status
signature verification with test fixture key
invalid signature rejection
SQLite ingest refuses invalid chain
```

These tests should be deterministic and runnable from a clean checkout.

## Operational notes

Evidence artifacts are local files in this project.

Operational risks:

```text
artifact directory missing
artifact directory not writable
disk full during write
partial write after process crash
manifest written before artifact flush
clock source inconsistency
operator edits artifact manually
tool reads artifact while writer is active
```

Implementation should prefer:

```text
explicit output directories
clear write errors
temporary file then atomic rename where practical
flush behavior documented
manifest generated after artifact completion
verification before ingest
```

## Relationship to protocol specification

Protocol findings and validation findings both become evidence, but they originate from different layers.

Protocol evidence examples:

```text
invalid magic
frame too large
unsupported protocol version
payload too short
unknown message type
```

Validation evidence examples:

```text
sequence replay
movement envelope violation
invalid aim vector
hit geometry violation
rate limit violation
```

The evidence schema should preserve the source layer instead of flattening all findings into one vague category.

## Review checklist

Before releasing evidence-related code, verify:

```text
Are required fields documented?
Are hashes computed from canonical content?
Is previous_hash included in record_hash computation?
Is record_hash excluded from its own computation?
Are schema versions checked?
Are invalid artifacts rejected clearly?
Are tamper cases tested?
Are limitations documented?
Are unsigned and invalid signatures distinguished?
Does the Unity viewer treat evidence as read-only?
Does SQLite ingest verify before storing?
```

## Summary

Tickline evidence should be explainable and tamper-evident.

```text
validation creates meaning
canonical records preserve meaning
hash chains protect ordering and content
manifests summarize artifacts
signatures may prove producer authenticity
verification protects investigation tools from corrupted artifacts
```

The purpose is disciplined security engineering: evidence should survive review.
