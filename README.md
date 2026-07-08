<div align="center">

# Tickline

### Deterministic security engineering portfolio project

A defensive system for authoritative simulation,
protocol hardening, evidence integrity, forensic replay, investigation
analytics, and reproducible engineering workflows.

[![CI](https://github.com/MichalPlanetaDev/tickline/actions/workflows/ci.yml/badge.svg)](https://github.com/MichalPlanetaDev/tickline/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/MichalPlanetaDev/tickline?display_name=tag&sort=semver)](https://github.com/MichalPlanetaDev/tickline/releases/latest)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)](cpp/)
[![Go](https://img.shields.io/badge/Go-developer_console-00ADD8?logo=go&logoColor=white)](tools/tickline-dev/)
[![Python](https://img.shields.io/badge/Python-analytics-3776AB?logo=python&logoColor=white)](tools/python/)
[![Unity](https://img.shields.io/badge/Unity-forensic_viewer-000000?logo=unity&logoColor=white)](unity/)
[![Docker](https://img.shields.io/badge/Docker-verification-2496ED?logo=docker&logoColor=white)](infra/docker/)
[![SQLite](https://img.shields.io/badge/SQLite-investigations-003B57?logo=sqlite&logoColor=white)](docs/investigation-storage.md)
[![License](https://img.shields.io/badge/license-see_LICENSE-informational)](LICENSE)

[Architecture](docs/architecture.md) ·
[Threat model](docs/threat-model.md) ·
[Developer console](docs/developer-console.md) ·
[Unity viewer](docs/unity-forensic-viewer.md) ·
[Analytics](docs/analytics.md) ·
[Latest release](https://github.com/MichalPlanetaDev/tickline/releases/latest)

</div>

---

## Overview

Tickline models a complete defensive trust chain around an authoritative,
deterministic simulation.

It accepts an untrusted byte stream, performs bounded protocol parsing and
strict command validation, admits valid commands into authoritative state,
records every outcome in canonical evidence, persists verified investigations,
supports deterministic replay, exposes a read-only Unity forensic workspace,
and produces explainable Python analytics.

The project is deliberately built as a multi-language engineering system rather
than a collection of disconnected demonstrations.

**Current release: `v1.0.0 — Portfolio release`**

The v1.0.0 milestone freezes the implemented architecture, aligns all component
versions, removes known compiler warnings, expands CI coverage, and presents the
system through a consolidated recruiter-facing project document.

## Why this project exists

Security-sensitive multiplayer and simulation systems cannot trust clients to
decide authoritative state.

Tickline demonstrates how to move responsibility across explicit trust
boundaries:

- raw network-like input is treated as hostile;
- parsing is bounded and schema-aware;
- commands are validated before state mutation;
- replay and sequence abuse are rejected;
- simulation behavior is deterministic;
- accepted and rejected submissions produce evidence;
- evidence is chained and verified against a trusted head;
- investigations are imported transactionally;
- replay reproduces behavior rather than merely displaying stored claims;
- analytics produces review inputs, not automatic verdicts;
- local verification produces persistent, integrity-checked artifacts.

## Architecture

~~~mermaid
flowchart LR
    A[Untrusted byte stream]
    B[Bounded frame reassembly]
    C[Strict envelope decoding]
    D[Identity and schema validation]
    E[Session replay protection]
    F[Authoritative command admission]
    G[Deterministic simulation]
    H[Canonical evidence record]
    I[SHA-256 evidence chain]
    J[Verified binary archive]
    K[SQLite investigation store]
    L[Native investigation bundle]
    M[Unity forensic replay]
    N[Python analytics]

    A --> B --> C --> D --> E --> F --> G
    F --> H
    G --> H
    H --> I --> J --> K --> L
    L --> M
    L --> N
~~~

The Go developer console operates across the repository as the verification and
workflow control plane:

~~~mermaid
flowchart TD
    CLI[tickline-dev]
    PLAN[Dependency-aware task planner]
    RUN[Process supervisor]
    LOGS[Per-stage logs]
    RESULT[result.json]
    MANIFEST[artifacts.json]
    VERIFY[Artifact verification]

    CLI --> PLAN --> RUN
    RUN --> LOGS
    RUN --> RESULT
    LOGS --> MANIFEST
    RESULT --> MANIFEST
    MANIFEST --> VERIFY
~~~

## Delivered capabilities

| Area | Implementation |
|---|---|
| Deterministic simulation | Fixed ticks, integer units, scheduled commands, bounded state, transactional advancement and stable state fingerprints |
| Command admission | Typed envelopes, identity checks, schema validation, timing rules, sequence protection and stable rejection codes |
| Protocol hardening | Fixed headers, bounded incremental parsing, exact payload decoding, malformed-input regression coverage and fuzz corpus testing |
| Evidence integrity | Canonical records for accepted and rejected submissions, SHA-256 chaining, trusted-head verification and strict archive decoding |
| Investigation storage | Migration-managed SQLite schema, transactional verified imports, filtering, pagination and corruption detection |
| Forensic replay | Trusted-state reconstruction, replay limits, mismatch detection and forged-outcome detection |
| Unity tooling | Read-only Editor viewer with transactional bundle loading, filtering, playback, seeking and integrity inspection |
| Analytics | Deterministic descriptive statistics, robust baselines, modified-z findings, explicit zero-MAD handling and human review metadata |
| Developer console | Diagnostics, plan inspection, TUI/plain/JSON output, workflow execution, cancellation and persisted run artifacts |
| Engineering workflow | CMake, CTest, sanitizers, Go race testing, Python tests, Docker smoke builds and GitHub Actions |

## Technology stack

| Layer | Technologies |
|---|---|
| Core systems | C++23, CMake, CTest |
| Security and integrity | SHA-256, canonical binary encoding, strict parsing, sanitizer and fuzz testing |
| Storage | SQLite |
| Tooling and orchestration | Go, Bubble Tea, Linux process groups |
| Analytics | Python standard library |
| Visualization | C#, Unity Editor tooling |
| Delivery and verification | Bash, Docker, GitHub Actions, WSL/Linux |

## Verification model

Tickline uses one repository-level quality gate:

    bash scripts/check-local.sh

The gate runs:

1. documentation validation;
2. C++ configuration, build and CTest;
3. AddressSanitizer and UndefinedBehaviorSanitizer tests;
4. Python unit tests;
5. Go unit tests, race detector and `go vet`;
6. Docker smoke-image verification.

A successful console execution persists evidence under:

    reports/check-local/<run-id>/

Each run contains:

- separate stdout and stderr logs for executed stages;
- combined per-stage logs;
- canonical `result.json`;
- `artifacts.json` with artifact kinds, sizes and SHA-256 digests.

Verify a run manifest with:

    build/tools/tickline-dev/tickline-dev \
      artifacts verify \
      reports/check-local/<run-id>/artifacts.json

The artifact manifest detects changes relative to the supplied manifest. It is
not a digital signature and does not establish authorship, provenance or trust
in the manifest itself.

## Quick start

### Prerequisites

The complete local gate expects:

- Linux or WSL;
- a C++23-capable compiler;
- CMake;
- SQLite development headers;
- Python 3;
- Go as declared by `tools/tickline-dev/go.mod`;
- Docker.

Unity verification additionally requires a locally installed and licensed
Windows Unity Editor.

### Clone and verify

    git clone https://github.com/MichalPlanetaDev/tickline.git
    cd tickline
    bash scripts/check-local.sh

### Build the C++ core

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

### Build the developer console

    bash scripts/build-tickline-dev.sh

    build/tools/tickline-dev/tickline-dev version
    build/tools/tickline-dev/tickline-dev doctor
    build/tools/tickline-dev/tickline-dev check --plan
    build/tools/tickline-dev/tickline-dev check --plain

### Run the release-readiness workflow

    build/tools/tickline-dev/tickline-dev \
      workflow run \
      --plain \
      release-readiness

### Run Unity EditMode verification

    bash scripts/check-unity.sh

The Unity runner stages a disposable project mirror on the Windows temporary
filesystem so repository assets are not modified by the Editor.

## Developer console

`tickline-dev` is not a second implementation of repository logic. It
orchestrates the existing C++, Python, Go, documentation, sanitizer and Docker
verification stages through one checked execution model.

Key commands:

    tickline-dev doctor
    tickline-dev check --plan
    tickline-dev check --plain
    tickline-dev check --json
    tickline-dev check --only docs,go
    tickline-dev workflow list
    tickline-dev workflow show release-readiness
    tickline-dev workflow run --plain release-readiness
    tickline-dev artifacts verify <artifacts.json>

The console supports automatic TUI selection, explicit plain or JSON output,
dependency-aware stage selection, process-tree cancellation and stable exit
codes.

## Investigation and replay flow

1. The C++ pipeline validates and executes a command.
2. A canonical evidence record is produced for the outcome.
3. Evidence records form a SHA-256-linked chain.
4. Verified archives are imported into SQLite transactionally.
5. The native exporter writes a schema-versioned investigation bundle.
6. Unity validates and loads the bundle into a read-only forensic workspace.
7. Python validates the same bundle before computing statistics or baselines.
8. Findings may receive explicit human review dispositions and evidence links.

This separation keeps evidence generation, persistence, visualization and
analysis independently testable.

## Repository layout

    cpp/
      include/tickline/        Public C++ interfaces
      src/                     Core implementation
      tests/                   Native tests

    docs/
      decisions/               Architecture decision records
      releases/                Versioned release notes

    infra/
      docker/                  Reproducible smoke environment

    scripts/
      checks/                  Individual verification stages

    tools/
      python/                  Investigation analytics
      tickline-dev/            Go developer console

    unity/
      TicklineForensics/       Unity forensic viewer

    unity-viewer/              Unity compatibility fixtures

## Security boundaries

Tickline permits defensive testing against code and artifacts owned by this
repository, including:

- malformed-input and parser-boundary testing;
- replay and sequence-abuse testing;
- invalid client-state claims;
- evidence-tampering tests;
- sanitizer and fuzz testing;
- deterministic replay verification;
- local Docker and Linux diagnostics.

The repository does not provide:

- third-party game targeting;
- commercial anti-cheat bypasses;
- credential theft or malware;
- stealth persistence;
- kernel evasion;
- DMA tooling;
- production-service attack instructions.

See [SECURITY.md](SECURITY.md) and the
[threat model](docs/threat-model.md) for the complete boundary.

## Explicit limitations

Tickline is a reference and portfolio system, not a production anti-cheat
service.

It does not include:

- a production TCP or UDP service;
- authenticated remote sessions;
- transport encryption or key management;
- digital signatures;
- a multi-tenant investigation API;
- hosted Unity execution in GitHub Actions;
- collision or hit validation;
- production monitoring or deployment infrastructure.

The Unity viewer is Editor-only and read-only. Analytics findings are review
inputs rather than automatic security verdicts.

## Documentation

| Document | Purpose |
|---|---|
| [Architecture](docs/architecture.md) | Component boundaries and responsibilities |
| [Simulation model](docs/simulation-model.md) | Deterministic world and command execution |
| [Command pipeline](docs/authoritative-command-pipeline.md) | Validation, admission, evidence and replay |
| [Protocol](docs/protocol.md) | Framing, decoding and stream parsing |
| [Protocol compatibility](docs/protocol-compatibility.md) | Native and managed contract rules |
| [Evidence integrity](docs/evidence-integrity.md) | Canonical records, chaining and archive verification |
| [Investigation storage](docs/investigation-storage.md) | SQLite persistence and query behavior |
| [Unity forensic viewer](docs/unity-forensic-viewer.md) | Bundle validation, replay and Editor UI |
| [Analytics](docs/analytics.md) | Statistics, baselines, outliers and reviews |
| [Developer console](docs/developer-console.md) | Go console architecture and execution model |
| [Threat model](docs/threat-model.md) | Defensive scope and trust assumptions |
| [Release process](docs/release-process.md) | Release invariants and procedure |

## Release history

<details>
<summary>Show milestone history</summary>

| Version | Milestone |
|---|---|
| `v0.1.0` | Blueprint and engineering skeleton |
| `v0.1.1` | Local workflow and CI repair |
| `v0.2.0` | Deterministic simulation core |
| `v0.3.0` | Developer console foundation |
| `v0.4.0` | Authoritative command pipeline |
| `v0.5.0` | Protocol boundary and parser hardening |
| `v0.6.0` | Investigation storage and query layer |
| `v0.7.0` | Unity forensic replay viewer |
| `v0.8.0` | Analytics and statistics |
| `v0.9.0` | Runtime diagnostics and service hardening |
| `v1.0.0` | Final portfolio release |

</details>

Detailed release notes are available in
[`docs/releases`](docs/releases/) and on the
[GitHub Releases page](https://github.com/MichalPlanetaDev/tickline/releases).

## Project status

`v1.0.0` is the completed portfolio baseline.

Future changes should be narrowly scoped maintenance, documented extensions or
clearly separated experimental work. Existing claims remain subordinate to
verified code, tests and release evidence.

## License

See [LICENSE](LICENSE) for the repository's licensing terms.
