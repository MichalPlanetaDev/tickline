# Changelog

## [1.0.0] - 2026-07-08

### Added

- Final recruiter-facing project README with architecture diagrams, capability
  matrix, verification model, quick-start workflow and documentation index.
- Complete GitHub Actions coverage for documentation, C++, sanitizers, Python,
  Go, race detection, static analysis and Docker verification.
- Final portfolio release notes and synchronized repository metadata.

### Changed

- Aligned CMake, C++, Python and Go version surfaces with v1.0.0.
- Reworked developer-console documentation into a concise operational reference.
- Clarified the project status, security boundaries and production limitations.
- Consolidated the release roadmap into the completed portfolio baseline.

### Fixed

- Removed the release-build false-positive warning around optional queue results.
- Removed signed-character conversion warnings from JSON string serialization.
- Removed duplicated v0.9.0 release-process metadata.
- Removed outdated current-release and planned-milestone claims.

### Verification

- Warning-free C++ release build.
- C++ unit tests and sanitizer configurations.
- Python unit tests.
- Go unit tests, race detector and `go vet`.
- Docker smoke build.
- Developer-console release-readiness workflow.
- Unity EditMode verification through the WSL-compatible runner.

### Limitations

- Tickline remains a defensive reference and portfolio system rather than a
  production anti-cheat service.
- Remote authentication, transport encryption, key management and digital
  signatures are not included.
- The Unity viewer remains Editor-only and read-only.
- Hosted Unity execution is not configured in GitHub Actions.

## [0.9.0] - 2026-07-07

### Added

- Go-based developer console with plain, JSON, and interactive terminal output.
- Declarative verification stages with dependency-aware execution planning.
- Repository and toolchain diagnostics through the `doctor` command.
- Operational workflow discovery, inspection, and execution.
- Release-readiness workflow metadata and execution support.
- Process-group cancellation with graceful termination and descendant cleanup.
- Per-run stdout, stderr, and combined stage logs.
- Canonical schema-version-2 JSON verification results.
- Schema-version-1 artifact manifests with file sizes and SHA-256 digests.
- Plain and JSON artifact verification through `artifacts verify`.
- Unit, integration, race-detector, and static-analysis coverage.

### Limitations

- The console is repository-local tooling, not a remote orchestration service.
- Artifact manifests are not digitally signed.
- Integrity verification does not prove authorship, provenance, or authenticity.
- Generated verification reports remain local and are excluded from Git.
- Hosted Unity execution and production deployment remain outside this release.

## [0.8.0] - 2026-07-06

### Added

- Strict schema-version-1 investigation-bundle validation in Python.
- Deterministic investigation, session, command-type, rejection-code, and tick statistics.
- Verified multi-investigation baselines with duplicate and candidate-leakage protection.
- Median and median-absolute-deviation metric distributions.
- Explainable modified-z and explicit zero-MAD-tolerance outlier findings.
- Human review dispositions, rationales, reviewers, timestamps, and evidence references.
- Deterministic schema-versioned JSON analytics reports.
- Analytics command-line interface and repository-local launcher.
- Documentation for trust boundaries, methodology, report contracts, and limitations.
- Python coverage for validation, statistics, baselines, outliers, reviews, reporting, CLI behavior, and launcher execution.

### Limitations

- Analytics findings are investigation inputs, not automatic security verdicts.
- No machine-learning model, automatic causal inference, production alerting, or remote analytics service is included.
- Baselines do not automatically compensate for different environments or workloads.
- Investigation bundles are not digitally signed and do not prove evidence authorship.
- Authentication, transport encryption, key management, collision validation, and production anti-cheat deployment remain out of scope.

## [0.7.0] - 2026-07-06

### Added

- Deterministic C++ investigation-bundle export from verified SQLite archives.
- Schema-versioned native-to-Unity JSON compatibility contract.
- Unity bundle loading and structural, identity, integer, digest, chain, and replay validation.
- Deterministic replay timeline with play, pause, step, seek, reset, and speed controls.
- Session, outcome, command, replay, archive, and evidence-chain inspection models.
- Transactional loading that preserves the active investigation after rejected replacement input.
- Unity Editor forensic replay window with filters, timeline selection, integrity status, and digest inspection.
- Unity EditMode coverage and a WSL-compatible Windows staging test runner.

### Limitations

- The viewer is Editor-only and read-only.
- Unity verification requires a locally installed and licensed Windows Unity Editor.
- Hosted Unity execution is not configured in GitHub Actions.
- Remote investigation services, authentication, encryption, digital signatures, evidence repair, collision validation, and production anti-cheat deployment remain out of scope.

## [0.6.0] - 2026-07-05

### Added

- Migration-managed SQLite investigation storage.
- Transactional and idempotent verified evidence-archive imports.
- Persisted session, submission, evidence, and replay metadata.
- Integrity constraints, pagination, filtering, and corruption detection.
- Rollback and storage-failure coverage.
- SQLite support in the Docker verification environment.

## [0.5.0] - 2026-07-04

### Added

- Strict fixed-header binary protocol framing and command-envelope decoding.
- Bounded incremental stream parsing with stable errors.
- Authoritative gateway integration, compatibility tests, malformed-input regression coverage, and a libFuzzer target.

### Limitations

- Production networking, authentication, encryption, signatures, durable investigation storage, and Unity visualization remain out of scope.

## v0.2.0 - Deterministic simulation core

Introduces the deterministic C++23 reference simulation used by later protocol, validation, replay, and concurrency work.

Added:

- explicit integer units for time, position, and velocity
- non-zero entity identifiers with stable ordering
- validated fixed-step simulation clock
- authoritative entity state
- deterministic velocity-command scheduling
- sequence and target-tick validation
- bounded position and velocity policies
- fractional displacement preservation
- transactional world advancement
- stable entity snapshots
- canonical big-endian world-state encoding
- versioned canonical-state schema
- deterministic 64-bit state fingerprints
- deterministic simulation contract documentation

Verification covers:

- unit conversion boundaries
- invalid clock and world configuration
- entity admission and duplicate rejection
- stale, regressing, and invalid commands
- fractional displacement
- scheduled command execution
- stable entity ordering
- insertion-order independence
- failed-advance state preservation
- exact canonical byte layout
- pending-command state encoding
- stable replay fingerprints
- standard and sanitizer builds

The state fingerprint uses FNV-1a only for deterministic comparison. It is not a cryptographic evidence mechanism.

Not included:

- networking
- protocol parsing
- collision or hit validation
- rollback or lag compensation
- multithreaded simulation
- cryptographic evidence hashing
- Unity replay visualization
- analytics

## v0.1.1 - Local workflow automation

Added a project-local quality gate so the common verification path no longer depends on copied manual command chains.

Added:

- `scripts/check-local.sh`
- `scripts/clean.sh`
- `justfile`
- README local workflow section

Fixed:

- updated `actions/checkout` to the supported Node runtime
- removed an accidentally tracked sanitized CMake build cache
- ignored `build-*` directories so host-generated CMake state cannot enter CI checkouts

The local quality gate runs documentation checks, CMake configure/build, CTest, sanitizer build, Python unittest discovery, and Docker smoke image build.

No runtime simulation, protocol parser, evidence writer, Unity viewer, or analytics engine was added in this patch release.

## v0.1.0 - Blueprint and engineering skeleton

Initial Tickline release.

This release defines the project as a deterministic security-engineering range for authoritative simulation, protocol hardening, cryptographic evidence integrity, forensic replay, and production-style workflow.

Added:

- project identity and proof targets
- repository skeleton
- GitHub labels, issues, and milestone structure
- architecture and trust-boundary documentation
- defensive threat model and safety boundary
- protocol specification
- evidence-integrity specification
- GitHub workflow policy
- debugging workflow
- release process
- security policy
- initial C++23 CMake build skeleton
- initial C++ version test
- sanitizer build option
- initial Python tooling skeleton
- Python package sanity test
- documentation sanity script
- Docker smoke build
- Docker build-context hygiene through .dockerignore
- GitHub Actions jobs for documentation, C++, sanitizers, Python, and Docker

Verification added:

- CMake configure
- C++ build
- C++ test execution through CTest
- AddressSanitizer and UndefinedBehaviorSanitizer build option
- Python unittest discovery
- documentation sanity check
- Docker smoke image build
- GitHub Actions CI workflow

Not included yet:

- runtime simulation
- protocol parser implementation
- evidence writer
- hash-chain verifier
- SQLite investigation database
- backend API
- Unity forensic viewer
- analytics engine
- Kubernetes, cloud, service mesh, or observability stack

This release intentionally establishes the engineering foundation before implementation begins.
