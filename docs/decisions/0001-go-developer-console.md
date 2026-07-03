# ADR 0001: Go Developer Console with Shared Check Execution

- Status: Accepted
- Date: 2026-07-02
- Scope: Tickline developer tooling

## Context

Tickline currently uses `scripts/check-local.sh` to run documentation, C++, sanitizer, Python, and Docker verification.

The shell workflow is useful because it is inspectable, available in minimal Linux environments, suitable for CI recovery, and does not require a compiled helper. Its interface becomes increasingly difficult to maintain as the project gains:

- more verification stages
- live subprocess output
- selective execution
- cancellation
- structured results
- responsive terminal layouts
- interactive log inspection
- machine-readable output
- terminal capability fallbacks

The project needs a richer developer interface without creating separate execution behavior for Bash, CI, JSON consumers, and the interactive terminal.

## Decision

Tickline will add a Go command named `tickline-dev`.

The command will provide three presentation modes over one execution engine:

- interactive terminal interface
- stable plain-text output
- versioned JSON output

Bubble Tea will be used only by the interactive renderer. Task definitions, process supervision, result aggregation, cancellation, logging, and exit-code handling must remain independent of Bubble Tea.

The existing shell workflow remains supported as a bootstrap, recovery, and diagnostic path.

## Shared task source

Check implementation must not be duplicated between Go, Bash, and CI.

Each verification stage will be represented by an executable stage script under:

```text
scripts/checks/
