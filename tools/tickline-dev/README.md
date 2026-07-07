# Tickline Developer Console

`tickline-dev` is the repository-local verification, diagnostics, and
workflow console for Tickline.

It provides one execution engine for line-oriented terminal output,
schema-versioned JSON reports, and an interactive Bubble Tea interface.
All modes use the same task manifest, dependency planner, process supervisor,
log store, result model, and artifact-integrity model.

## Build

From the repository root:

    bash scripts/build-tickline-dev.sh

The binary is written to:

    build/tools/tickline-dev/tickline-dev

The build script also verifies the reported console version.

## Install

Install into `${XDG_BIN_HOME}` or `~/.local/bin`:

    bash scripts/install-tickline-dev.sh

Install into a specific directory:

    bash scripts/install-tickline-dev.sh /desired/bin/directory

## Core commands

Display help and version information:

    tickline-dev help
    tickline-dev version

Inspect repository and toolchain readiness:

    tickline-dev doctor
    tickline-dev doctor --json

Validate the execution plan without running it:

    tickline-dev check --plan

Run the complete default verification plan:

    tickline-dev check
    tickline-dev check --plain
    tickline-dev check --json

Select or exclude stages:

    tickline-dev check --only docs,go
    tickline-dev check --skip docker

## Operational workflows

List workflows:

    tickline-dev workflow list

Inspect one workflow:

    tickline-dev workflow show release-readiness

Execute the release-readiness workflow:

    tickline-dev workflow run --plain release-readiness

Workflow execution resolves to the same checked task plan used by the
standard `check` command.

## Artifact verification

Every executed run creates:

    reports/check-local/<run-id>/

The run directory contains:

- stdout, stderr, and combined logs for every executed stage;
- `result.json`, the canonical schema-versioned result;
- `artifacts.json`, the stable artifact integrity inventory.

Verify a manifest:

    tickline-dev artifacts verify \
      reports/check-local/<run-id>/artifacts.json

Emit the verification result as JSON:

    tickline-dev artifacts verify \
      --json \
      reports/check-local/<run-id>/artifacts.json

The manifest records artifact paths, kinds, sizes, and SHA-256 digests.
Verification detects changes relative to the supplied manifest. It is not a
digital signature and does not establish authorship, provenance, or trust in
the manifest itself.

## Output selection

An interactive terminal uses the TUI automatically:

    tickline-dev check

Force a specific mode:

    tickline-dev check --tui
    tickline-dev check --plain
    tickline-dev check --json

Redirected output and unsupported terminals use plain output.

`--plain`, `--json`, and `--tui` are mutually exclusive. They cannot be
combined with `--plan`.

## Exit codes

| Code | Meaning |
| ---: | --- |
| `0` | Verification passed |
| `1` | A verification or integrity check failed |
| `2` | Invalid command-line usage or plan selection |
| `3` | Internal console, repository, or artifact failure |
| `130` | Execution was cancelled |

## Cancellation

Cancellation is propagated to the active Linux process group. The console
sends `SIGTERM`, waits for the configured grace period, and sends `SIGKILL`
to surviving descendants.

## TUI controls

| Key | Action |
| --- | --- |
| `j`, `Down` | Select the next stage |
| `k`, `Up` | Select the previous stage |
| `g`, `Home` | Select the first stage |
| `G`, `End` | Select the last stage |
| `q`, `Esc` | Cancel an active run or leave a completed run |
| `Ctrl+C` | Request cancellation |
| `Enter` | Leave a completed run |

## Development verification

From the Go module:

    cd tools/tickline-dev
    gofmt -w .
    go test ./...
    go test -race ./...
    go vet ./...

From the repository root:

    bash scripts/check-local.sh
