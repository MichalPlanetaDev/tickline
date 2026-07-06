package cli

import (
	"context"
	"fmt"
	"io"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/version"
)

const (
	ExitSuccess       = 0
	ExitCheckFailed   = 1
	ExitInvalidUsage  = 2
	ExitInternalError = 3
	ExitInterrupted   = 130
)

type Dependencies struct {
	Context           context.Context
	Stdin             io.Reader
	Stdout            io.Writer
	Stderr            io.Writer
	WorkingDirectory  string
	TerminalAvailable bool
}

func Run(args []string, dependencies Dependencies) int {
	if dependencies.Stdout == nil ||
		dependencies.Stderr == nil {
		return ExitInternalError
	}

	if dependencies.Context == nil {
		dependencies.Context = context.Background()
	}

	if len(args) == 0 {
		printHelp(dependencies.Stdout)
		return ExitSuccess
	}

	switch args[0] {
	case "help", "-h", "--help":
		printHelp(dependencies.Stdout)
		return ExitSuccess

	case "version", "-v", "--version":
		fmt.Fprintf(
			dependencies.Stdout,
			"tickline-dev %s\n",
			version.Current,
		)

		return ExitSuccess

	case "check":
		return runCheck(args[1:], dependencies)

	case "workflow":
		return runWorkflow(args[1:], dependencies)

	default:
		fmt.Fprintf(
			dependencies.Stderr,
			"unknown command: %s\n\n",
			args[0],
		)

		printHelp(dependencies.Stderr)

		return ExitInvalidUsage
	}
}

func printHelp(output io.Writer) {
	_, _ = fmt.Fprintln(output, `Tickline developer console

Usage:
  tickline-dev <command>

Commands:
  check       Run project verification
  workflow    Inspect named operational workflows
  version     Print the developer-console version
  help        Show this help

Check options:
  --plan          Validate and display the execution plan
  --plain         Force line-oriented plain output
  --tui           Force the interactive terminal interface
  --json          Emit one versioned JSON result document
  --only <ids>    Select comma-separated stages
  --skip <ids>    Skip comma-separated stages

Workflow commands:
  workflow list
  workflow show <workflow-id>

Mode selection:
  A compatible interactive terminal uses the TUI automatically.
  Redirected output and unsupported terminals use plain output.

Global options:
  -h, --help       Show help
  -v, --version    Print the version`)
}
