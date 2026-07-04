# Authoritative Command Pipeline

## Purpose

The authoritative command pipeline is the boundary between an untrusted
command claim and deterministic simulation state.

A command is accepted only after:

1. stateless envelope validation;
2. session replay validation;
3. deterministic translation;
4. authoritative world admission.

## Command envelope

`CommandEnvelope` contains:

```text
schema_version
command_type
client_id
session_id
sequence
target_tick
payload
```

The current payload is `SetVelocityPayload`, containing an entity identifier
and two-dimensional integer velocity.

The envelope is currently a typed in-process boundary. It is not yet decoded
from network bytes.

## Stateless validation

Validation covers:

- supported command schema;
- supported command type;
- non-zero client and session identifiers;
- authenticated client identity;
- active session identity;
- non-zero sequence;
- future target tick;
- bounded future-tick distance;
- configured velocity limits.

Validation order is deterministic, so a command that violates several rules
still produces one stable rejection code.

## Session replay protection

A `CommandSession` tracks the highest sequence accepted for one client/session
pair.

The session distinguishes:

```text
sequence == highest accepted  duplicate
sequence < highest accepted   regression
sequence > highest accepted   eligible
```

Sequence gaps are permitted.

Session evaluation does not mutate replay state. The sequence is committed
only after authoritative world admission succeeds.

## Authoritative world admission

The world remains responsible for simulation-specific invariants:

- target tick must be in the future;
- entity must exist;
- entity sequence must increase;
- entity target tick must not regress;
- velocity must satisfy world limits.

A world-level rejection does not consume the session sequence.

## Transactional submission

`AuthoritativeCommandPipeline::submit()` performs:

```text
capture pre-submission state
validate session and envelope
translate command
submit to authoritative world
commit session sequence only on acceptance
append one evidence record
```

A rejected submission leaves both session sequence and pending-command count
unchanged.

An accepted submission advances the session sequence to the command sequence
and increases the pending-command count by one.

## Evidence

Every normal submission produces evidence, including validation rejection,
replay rejection, world rejection, and acceptance.

Each evidence entry records:

- pre-submission world fingerprint;
- observed simulation tick;
- complete command envelope;
- session sequence before and after;
- pending-command count before and after;
- stable rejection code;
- world queue outcome.

## SHA-256 evidence chain

Evidence record schema version 2 uses a fixed 160-byte big-endian encoding.

Each record stores the SHA-256 digest of the previous encoded record. The first
record uses an all-zero digest.

Verification requires an independently trusted expected chain head. The head
declared inside an archive is not trusted by itself.

## Binary archive

Archive schema version 1 uses the `TLCA` magic value and a 56-byte header.

The decoder rejects:

- truncated headers or records;
- unsupported schemas;
- invalid record sizes;
- non-zero reserved fields;
- count and size overflow;
- trailing data;
- malformed record invariants;
- evidence-chain mismatch;
- trusted-head mismatch.

## Deterministic forensic replay

Replay receives trusted initial session state, trusted initial world state,
evidence records, and a trusted chain head.

For every record, replay:

1. reconstructs the observed tick;
2. verifies pre-submission session state;
3. verifies pending-command state;
4. verifies the world fingerprint;
5. resubmits the recorded envelope;
6. compares regenerated evidence with archived evidence.

Replay can detect semantically forged outcomes even when a modified internal
chain has been recomputed, provided the investigator has trusted initial state
or the correct external head.

## Limitations

`v0.4.0` does not provide:

- network framing or byte parsing;
- transport authentication;
- encryption;
- digital signatures;
- key management;
- concurrent session registry;
- crash-consistent append-only storage;
- database ingestion;
- Unity visualization.
