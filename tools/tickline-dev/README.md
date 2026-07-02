# Tickline Developer Console

`tickline-dev` is the Go-based developer console for Tickline.

It will provide:

- shared verification orchestration
- plain terminal output
- versioned JSON output
- an optional responsive Bubble Tea interface
- cancellation and subprocess supervision
- complete per-stage logs

The console does not replace the portable shell fallback.

Architecture and behavioral contracts are defined in:

- `docs/developer-console.md`
- `docs/decisions/0001-go-developer-console.md`

The Go module and executable skeleton will be added in the next implementation unit.
