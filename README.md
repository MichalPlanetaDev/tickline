# Tickline

Deterministic security-engineering range for authoritative simulation, protocol hardening, cryptographic evidence integrity, forensic replay, and production-style workflow project.

Tickline is a lawful systems/security portfolio project. Its purpose is to demonstrate how a game-security platform can be designed around deterministic simulation, untrusted-client assumptions, hardened protocol boundaries, tamper-evident evidence, runtime diagnostics, and disciplined engineering workflow.

This is not a cheat project, bypass project, malware project, or reverse-engineering target for third-party software. All adversarial testing is performed against owned code inside this repository.

## Project thesis

Modern multiplayer security is not a single detector. It is a chain of engineering decisions:

```text
client is untrusted
        ↓
protocol boundary is explicit
        ↓
server owns simulation state
        ↓
claims are validated against deterministic rules
        ↓
telemetry is structured
        ↓
evidence is integrity-protected
        ↓
operators can investigate decisions
        ↓
runtime behavior is observable and reproducible
```

Tickline is designed to make that chain visible.

## Current status

Current development release:

**v0.2.0 — Deterministic simulation core**

The C++ core now provides fixed-duration ticks, integer simulation units, stable entity ordering, scheduled velocity commands, bounded integration, replayable state, canonical serialization, and deterministic state fingerprints.

Networking, protocol parsing, evidence writing, cryptographic integrity, Unity visualization, and analytics remain outside this release.

## What Tickline is meant to prove

| Area | Proof target |
|---|---|
| C++ systems programming | Deterministic simulation core, ownership discipline, parser implementation, tests, sanitizers |
| C# and Unity | Forensic replay viewer, timeline inspection, debugging UI, managed tooling around native/server data |
| Game security | Server authority, invalid client claims, timing abuse, replay attempts, malformed protocol input |
| Protocol engineering | Versioned framed protocol, parser hardening, malformed input handling, fuzzable boundaries |
| Cryptography | Hash-chained evidence, signed manifests, tamper detection, replay-protection design |
| Linux | CLI workflow, permissions awareness, process/runtime diagnostics, logs, Docker, service behavior |
| DevOps | CI/CD quality gates, Docker builds, reproducible commands, release hygiene, GitHub workflow |
| Networking | TCP/UDP design tradeoffs, framing, ordering, latency windows, timeout behavior, service contracts |
| Mathematics and physics | Fixed timestep simulation, vectors, ray tests, swept movement, tolerance windows, spatial structures |
| Data and analytics | Structured telemetry, SQL queries, statistical summaries, anomaly analysis, false-positive discussion |
| Security engineering | Threat model, safe adversarial testing, defensive scope, evidence review, no offensive tooling |
| Professional workflow | Issues, labels, milestones, branches, PR-style changes, changelog, release notes, documentation |

## Intended architecture

```text
+--------------------+        +-------------------------+
| Controlled clients | -----> | Protocol boundary       |
| Unity / bots       |        | framing + validation    |
+--------------------+        +-----------+-------------+
                                          |
                                          v
                              +-----------+-------------+
                              | C++ authoritative core  |
                              | deterministic ticks     |
                              | physics + validation    |
                              +-----------+-------------+
                                          |
                                          v
                              +-----------+-------------+
                              | Evidence pipeline       |
                              | telemetry + hash chain  |
                              | signed artifacts        |
                              +-----------+-------------+
                                          |
                    +---------------------+---------------------+
                    |                                           |
                    v                                           v
        +-----------+-------------+                 +-----------+-------------+
        | Investigation storage   |                 | Runtime diagnostics     |
        | SQLite / later service  |                 | logs, metrics, traces   |
        +-----------+-------------+                 +-----------+-------------+
                    |                                           |
                    v                                           v
        +-----------+-------------+                 +-----------+-------------+
        | Python analytics tools  |                 | Linux / Docker / CI     |
        | reports + statistics    |                 | reproducible workflow   |
        +-----------+-------------+                 +-------------------------+
                    |
                    v
        +-----------+-------------+
        | Unity forensic viewer   |
        | replay + timeline UI    |
        +-------------------------+
```

The central rule is that the server owns truth. Clients may request actions, but they do not authoritatively decide movement, hits, timing, or evidence.

## Engineering boundaries

Tickline is allowed to include:

```text
owned-code adversarial testing
malformed packet tests
parser fuzzing
protocol replay tests
server-side validation
hash-chain tamper detection
signed local artifacts
Dockerized local runtime
Linux debugging notes
CI quality gates
Unity forensic visualization
Python statistical analysis
technical writeups
```

Tickline must not include:

```text
cheat loaders
bypass tooling
DLL injection against third-party games
kernel evasion
DMA tooling
credential theft
malware
live-service targeting
real anti-cheat bypass research
instructions for evading production security systems
```

The project is defensive by design.

## Planned milestones

| Version | Milestone | Scope |
|---|---|---|
| `v0.1.0` | Blueprint and engineering skeleton | Repository structure, docs, CI skeleton, GitHub workflow |
| `v0.2.0` | C++ deterministic simulation core | Fixed ticks, state model, replayable simulation, tests |
| `v0.3.0` | Protocol parser and hardening | Framing, versioning, malformed input tests, fuzz target |
| `v0.4.0` | Authoritative validation | Movement, timing, sequence, physics and state validation |
| `v0.5.0` | Evidence integrity | Hash-chained telemetry, signed manifests, tamper checks |
| `v0.6.0` | Investigation storage and API | SQLite schema, service contracts, query layer, health checks |
| `v0.7.0` | Unity forensic replay viewer | Timeline replay, state inspection, evidence visualization |
| `v0.8.0` | Analytics and statistics | Python reports, baselines, outlier scoring, false-positive analysis |
| `v0.9.0` | Runtime diagnostics and hardening | Docker, Linux diagnostics, sanitizer/fuzz workflow, observability notes |
| `v1.0.0` | Final portfolio release | Screenshots, release notes, documentation freeze, reproducible demo |

The roadmap may be refined, but features should not be added only because they sound impressive. Every milestone must strengthen the system.

## GitHub workflow

Expected workflow:

```text
issue defines scoped work
branch implements one coherent change
commit message describes the engineering change
CI validates the branch
pull request records review context
merge preserves clean history
tag marks a release boundary
release notes explain what changed and why
```

Branch examples:

```text
docs/project-identity
docs/threat-model
feature/cpp-simulation-core
feature/protocol-parser
hardening/parser-fuzz-target
infra/docker-runtime
```

Commit examples:

```text
Define Tickline project identity
Add architecture and trust-boundary documentation
Implement deterministic tick simulation
Add protocol parser malformed-input tests
Add hash-chain evidence verification
```

## Local workflow

The main local verification command is:

    bash scripts/check-local.sh

It runs the same baseline checks expected from the early project:

- documentation sanity check
- CMake configure
- C++ build
- CTest
- sanitizer build
- Python unittest discovery
- Docker smoke image build

For a faster local pass during small edits:

    SKIP_SANITIZERS=1 SKIP_DOCKER=1 bash scripts/check-local.sh

If `just` is installed, the same workflow is available as:

    just check
    just check-fast
    just clean

The script is the source of truth. Long manual command chains should not be copied around unless a failure is being isolated.

## Quality gates

The final project should include quality gates for:

```text
CMake configure
C++ build
C++ tests
clang-format
clang-tidy where practical
AddressSanitizer
UndefinedBehaviorSanitizer
parser fuzz target build
Python tests
documentation sanity checks
Docker build
Docker smoke test
```

Quality gates should be useful, not decorative. A failing gate must indicate a real engineering problem.

## Development environment

Primary target environment:

```text
Linux / WSL2
C++23
CMake
Ninja
Python 3
Docker
GitHub CLI
Unity LTS or current stable Unity editor for the viewer
```

The repository should remain buildable from a clean checkout once implementation begins.

## Documentation map

Planned documentation:

| Document | Purpose |
|---|---|
| `docs/architecture.md` | System design, component boundaries, data flow, trust boundaries |
| `docs/simulation-model.md` | Deterministic units, tick behavior, command ordering, canonical state, and failure guarantees |
| `docs/threat-model.md` | Assets, attackers, abuse cases, prohibited scope, defensive constraints |
| `docs/protocol.md` | Framing, message format, parser behavior, versioning, malformed input handling |
| `docs/evidence-integrity.md` | Hash chain, artifact signing, tamper detection, evidence limitations |
| `docs/github-workflow.md` | Issues, labels, branches, PRs, commits, releases, hotfixes, bisect/revert rules |
| `docs/debugging-workflow.md` | Linux diagnostics, logs, process inspection, sanitizer output, failure triage |
| `docs/release-process.md` | Release checklist, tags, changelog, screenshots, demo verification |
| `SECURITY.md` | Safe-use policy and vulnerability reporting boundary |
| `CHANGELOG.md` | Versioned release history grounded in Git history |

## Review path

At `v1.0.0`, a reviewer should be able to:

```text
read the README and understand the system
inspect the architecture and threat model
run the local demo
see CI passing
inspect tests and sanitizer configuration
review evidence-integrity behavior
open the Unity forensic viewer
read release notes
understand what is intentionally out of scope
```

The intended impression is not breadth for its own sake. The intended impression is disciplined engineering across security, simulation, runtime, tooling, and evidence.

## License

MIT.
