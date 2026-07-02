# Changelog

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
