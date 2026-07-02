# Changelog

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
