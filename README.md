# Tickline

Tickline is a lawful, defensive security-engineering range built around deterministic authoritative simulation, strict command admission, evidence integrity, forensic replay, and reproducible engineering workflow.

It is not a cheat project, bypass project, malware project, or reverse-engineering target for third-party software. Adversarial tests operate only against code and artifacts owned by this repository.

## Current release

**v0.5.0 — Protocol boundary and parser hardening**

The current C++ core now includes deterministic authoritative simulation, typed command admission, replay protection, SHA-256-linked evidence, verified evidence archives, deterministic forensic replay, strict 32-byte big-endian frame decoding, bounded command-envelope deserialization, stable parser errors, incremental stream reassembly, compatibility tests, malformed-input regression coverage, and a libFuzzer target.

The repository also includes the Go-based `tickline-dev` verification console, Python support-package scaffolding, Docker checks, sanitizer builds, and GitHub workflow documentation.

## Security thesis

Tickline models this trust chain:

```text
untrusted byte stream
        |
        v
bounded frame reassembly
        |
        v
strict frame and payload decoding
        |
        v
typed command boundary
        |
        v
identity, schema, sequence, timing, and payload validation
        |
        v
authoritative world admission
        |
        v
deterministic simulation state
        |
        v
canonical evidence record
        |
        v
SHA-256 evidence chain
        |
        v
verified binary archive
        |
        v
deterministic forensic replay
```

A client may request an action. It does not authoritatively decide simulation state, validation results, or evidence.

## Implemented components

### Deterministic simulation

The C++ simulation core supports:

- fixed-duration ticks;
- checked elapsed-time progression;
- non-zero entity identifiers;
- integer millimeter and microsecond units;
- scheduled velocity commands;
- per-entity sequence and target-tick tracking;
- bounded velocity and position;
- fractional displacement preservation;
- atomic world advancement;
- canonical state encoding;
- deterministic state fingerprints.

The state fingerprint is intended for deterministic comparison and replay checks. It is not a cryptographic authentication mechanism.

### Command envelope and validation

Each command envelope carries:

- command schema version;
- command type;
- client identifier;
- session identifier;
- session sequence;
- target simulation tick;
- entity identifier;
- typed velocity payload.

The stateless validator rejects unsupported schemas and command types, invalid or mismatched identities, zero sequences, stale ticks, excessive future ticks, and out-of-range velocity.

### Session replay protection

A `CommandSession` owns the highest accepted sequence for one authenticated client/session pair.

It distinguishes:

- duplicate sequence;
- sequence regression;
- valid strictly increasing sequence.

Sequence gaps are allowed. A sequence is committed only after authoritative world admission succeeds.

### Authoritative submission pipeline

`AuthoritativeCommandPipeline` coordinates:

1. session-level validation;
2. replay checks;
3. translation into a simulation command;
4. authoritative world admission;
5. session-sequence commit;
6. evidence generation.

A rejected submission does not commit session replay state. World-level rejections do not consume a session sequence.

### Evidence integrity

Every submission produces a fixed-size canonical evidence record, including rejected submissions.

Evidence records contain:

- pre-submission world fingerprint;
- observed simulation tick;
- full command envelope;
- session sequence before and after;
- pending-command count before and after;
- stable rejection code;
- raw queue outcome.

Records are linked with SHA-256. Verification requires an independently trusted expected chain head.

The chain detects modification relative to that trusted head. It does not prove who produced the archive and is not a digital signature.

### Evidence archives

The binary archive format provides:

- archive magic and schema;
- embedded record schema and size;
- record count;
- declared SHA-256 chain head;
- fixed-size canonical records;
- exact-length validation;
- truncation detection;
- trailing-data rejection;
- strict record decoding;
- chain verification;
- trusted-head comparison.

### Forensic replay

The evidence replayer accepts:

- a trusted initial `CommandSession`;
- a trusted initial `World`;
- verified evidence records;
- a trusted evidence-chain head.

It reconstructs observed tick progression and command submission, then compares regenerated records with archived records.

Replay detects:

- invalid chain linkage;
- tick regression;
- excessive replay work;
- simulation-advance failure;
- initial-session mismatch;
- pending-command mismatch;
- world-state mismatch;
- forged submission outcomes.

## Important limitations

The current release does not provide:

- a production TCP or UDP server;
- socket lifecycle or connection management;
- authenticated sessions;
- transport encryption;
- digital signatures or key management;
- crash-consistent append-only evidence storage;
- a concurrent session registry;
- database ingestion;
- Unity visualization;
- collision or hit validation;
- production anti-cheat deployment.

The protocol parser secures the owned byte boundary. It does not authenticate clients, encrypt traffic, sign evidence, or establish producer attribution.

## Repository layout

```text
cpp/
  include/tickline/
    command/
    security/
    simulation/
  src/
  tests/

docs/
  decisions/
  releases/

infra/
  docker/

scripts/
  checks/

tools/
  python/
  tickline-dev/

unity-viewer/
```

## Local verification

Run the complete repository gate:

```bash
bash scripts/check-local.sh
```

Run a faster local pass:

```bash
SKIP_SANITIZERS=1 SKIP_DOCKER=1 \
  bash scripts/check-local.sh
```

Build and test only the C++ core:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the sanitizer configuration:

```bash
cmake \
  -S . \
  -B build-sanitized \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTICKLINE_ENABLE_SANITIZERS=ON

cmake --build build-sanitized --parallel
ctest --test-dir build-sanitized --output-on-failure
```

Build the developer console:

```bash
bash scripts/build-tickline-dev.sh
```

## Developer console

`tickline-dev` provides one execution engine for:

- plain terminal output;
- versioned JSON reports;
- interactive terminal UI;
- dependency-aware stage selection;
- process-group cancellation;
- per-stage logs;
- canonical `result.json` artifacts.

Examples:

```bash
tickline-dev version
tickline-dev check --plan
tickline-dev check --plain
tickline-dev check --json
tickline-dev check --only cpp,go
```

## Release history

| Version | Milestone | Delivered scope |
|---|---|---|
| `v0.1.0` | Blueprint and engineering skeleton | Repository structure, architecture, threat model, CI and workflow foundation |
| `v0.1.1` | Local workflow and CI repair | Reproducible local checks and corrected CI behavior |
| `v0.2.0` | Deterministic simulation core | Fixed ticks, integer state, scheduled commands, canonical state and tests |
| `v0.3.0` | Developer console foundation | Go verification console, shared manifest, TUI, JSON reports, cancellation and logs |
| `v0.4.0` | Authoritative command pipeline | Command admission, replay protection, evidence archives, SHA-256 chain and forensic replay |
| `v0.5.0` | Protocol boundary and parser hardening | Strict framing, bounded decoding, incremental stream parsing, malformed-input tests and fuzzing |

## Planned milestones

| Version | Milestone | Intended scope |
|---|---|---|
| `v0.6.0` | Investigation storage and query layer | SQLite schema, import path, queries and service contracts |
| `v0.7.0` | Unity forensic replay viewer | Timeline replay, state inspection and evidence visualization |
| `v0.8.0` | Analytics and statistics | Python reports, baselines, outlier analysis and false-positive review |
| `v0.9.0` | Runtime diagnostics and service hardening | Docker runtime, diagnostics, observability and operational failure tests |
| `v1.0.0` | Portfolio release | Reproducible demonstration, screenshots, documentation freeze and final review |

The roadmap is subordinate to verified implementation. Features are not considered delivered until code, tests, documentation, and release metadata agree.

## Documentation

- `docs/architecture.md` — current system boundaries and component responsibilities;
- `docs/simulation-model.md` — deterministic world and command execution model;
- `docs/authoritative-command-pipeline.md` — command envelope, validation, submission, evidence, and replay;
- `docs/evidence-integrity.md` — record chain and archive integrity model;
- `docs/protocol.md` — implemented framing, decoding, stream parsing, and compatibility rules;
- `docs/threat-model.md` — defensive scope and trust assumptions;
- `docs/developer-console.md` — Go verification-console architecture;
- `docs/debugging-workflow.md` — local diagnosis workflow;
- `docs/github-workflow.md` — issue, branch, review, and release workflow;
- `docs/release-process.md` — release invariants and exact release procedure;
- `docs/releases/v0.4.0.md` — release notes for the current milestone.

## Defensive scope

Permitted work includes:

- malformed-input testing against Tickline;
- replay and sequence-abuse tests;
- invalid command claims;
- evidence-tampering tests;
- deterministic replay verification;
- sanitizer and fuzz testing;
- local Docker and Linux diagnostics;
- defensive technical documentation.

The repository must not include malware, credential theft, third-party game targeting, commercial anti-cheat bypasses, stealth persistence, kernel evasion, DMA tooling, or instructions for attacking production services.
