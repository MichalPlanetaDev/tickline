# Tickline Developer Console

`tickline-dev` is the Go-based local verification console for Tickline.

It provides one execution engine for plain terminal output, versioned JSON
reporting, and an interactive Bubble Tea interface. Every execution uses the
same task manifest, dependency planner, process supervisor, log store, and
result model.

## Build

From the repository root:

```bash
bash scripts/build-tickline-dev.sh
```

The binary is written to:

```text
build/tools/tickline-dev/tickline-dev
```

The build script verifies that the binary reports the expected release
version.

## Install

Install into `${XDG_BIN_HOME}` or `~/.local/bin`:

```bash
bash scripts/install-tickline-dev.sh
```

Install into a specific directory:

```bash
bash scripts/install-tickline-dev.sh /desired/bin/directory
```

## Commands

Display help:

```bash
tickline-dev help
```

Display the version:

```bash
tickline-dev version
```

Validate and display the execution plan without running it:

```bash
tickline-dev check --plan
```

Run selected stages:

```bash
tickline-dev check --only docs,go
```

Skip selected stages:

```bash
tickline-dev check --skip docker
```

## Output modes

An interactive terminal uses the TUI automatically:

```bash
tickline-dev check
```

Force the TUI:

```bash
tickline-dev check --tui
```

Force line-oriented output:

```bash
tickline-dev check --plain
```

Emit one JSON document to standard output:

```bash
tickline-dev check --json
```

Redirected output and unsupported terminals automatically use plain mode.

`--plain`, `--json`, and `--tui` are mutually exclusive. They cannot be
combined with `--plan`.

## Exit codes

| Code | Meaning |
| ---: | --- |
| `0` | All selected checks passed |
| `1` | A verification stage failed |
| `2` | Invalid command-line usage or plan selection |
| `3` | Internal console or artifact failure |
| `130` | Execution was cancelled |

## Verification artifacts

Each execution creates a unique directory under:

```text
reports/check-local/<run-id>/
```

The directory contains:

- separate standard-output and standard-error logs for each executed stage;
- a combined log for each executed stage;
- `result.json`, the canonical versioned run result.

Generated reports are intentionally excluded from Git.

## TUI controls

| Key | Action |
| --- | --- |
| `j`, `Down` | Select next stage |
| `k`, `Up` | Select previous stage |
| `g`, `Home` | Select first stage |
| `G`, `End` | Select last stage |
| `q`, `Esc` | Cancel an active run or leave a completed run |
| `Ctrl+C` | Request cancellation |
| `Enter` | Leave a completed run |

Cancellation is propagated to the active Linux process group. Tickline sends
`SIGTERM`, waits for the configured grace period, and then sends `SIGKILL` to
surviving descendants.

## Development verification

From the Go module:

```bash
cd tools/tickline-dev

gofmt -w .
go test ./...
go test -race ./...
go vet ./...
```

From the repository root:

```bash
bash scripts/check-local.sh
```
