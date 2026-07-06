package cli

import (
	"fmt"
	"strings"
)

type workflowRunOptions struct {
	Identifier string
	Plain      bool
	JSON       bool
	TUI        bool
}

func runWorkflowRun(
	args []string,
	dependencies Dependencies,
) int {
	options, help, err := parseWorkflowRunArguments(args)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"workflow run: %v\n",
			err,
		)

		return ExitInvalidUsage
	}

	if help {
		printWorkflowRunHelp(dependencies.Stdout)
		return ExitSuccess
	}

	plan, err := resolveWorkflowPlan(
		options.Identifier,
		dependencies.WorkingDirectory,
	)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"resolve workflow: %v\n",
			err,
		)

		return ExitInvalidUsage
	}

	stageIDs := plan.StageIDs()
	if len(stageIDs) == 0 {
		fmt.Fprintf(
			dependencies.Stderr,
			"resolve workflow: workflow %q has no executable stages\n",
			options.Identifier,
		)

		return ExitInvalidUsage
	}

	checkArguments := []string{
		"--only",
		strings.Join(stageIDs, ","),
	}

	switch {
	case options.Plain:
		checkArguments = append(
			checkArguments,
			"--plain",
		)

	case options.JSON:
		checkArguments = append(
			checkArguments,
			"--json",
		)

	case options.TUI:
		checkArguments = append(
			checkArguments,
			"--tui",
		)
	}

	return runCheck(
		checkArguments,
		dependencies,
	)
}

func parseWorkflowRunArguments(
	args []string,
) (workflowRunOptions, bool, error) {
	var options workflowRunOptions

	for _, argument := range args {
		switch argument {
		case "--plain":
			if options.Plain {
				return workflowRunOptions{}, false, fmt.Errorf(
					"--plain was provided more than once",
				)
			}

			options.Plain = true

		case "--json":
			if options.JSON {
				return workflowRunOptions{}, false, fmt.Errorf(
					"--json was provided more than once",
				)
			}

			options.JSON = true

		case "--tui":
			if options.TUI {
				return workflowRunOptions{}, false, fmt.Errorf(
					"--tui was provided more than once",
				)
			}

			options.TUI = true

		case "-h", "--help":
			return workflowRunOptions{}, true, nil

		default:
			if strings.HasPrefix(argument, "-") {
				return workflowRunOptions{}, false, fmt.Errorf(
					"unknown option %q",
					argument,
				)
			}

			if options.Identifier != "" {
				return workflowRunOptions{}, false, fmt.Errorf(
					"unexpected argument %q",
					argument,
				)
			}

			options.Identifier = argument
		}
	}

	explicitModes := 0

	for _, enabled := range []bool{
		options.Plain,
		options.JSON,
		options.TUI,
	} {
		if enabled {
			explicitModes++
		}
	}

	if explicitModes > 1 {
		return workflowRunOptions{}, false, fmt.Errorf(
			"--plain, --json, and --tui are mutually exclusive",
		)
	}

	if options.Identifier == "" {
		return workflowRunOptions{}, false, fmt.Errorf(
			"workflow identifier is required",
		)
	}

	return options, false, nil
}

func printWorkflowRunHelp(
	output interface {
		Write([]byte) (int, error)
	},
) {
	_, _ = fmt.Fprintln(output, `Usage:
  tickline-dev workflow run [--plain|--json|--tui] <workflow-id>

Output selection:
  A compatible interactive terminal uses the TUI automatically.
  Redirected output and unsupported terminals use plain output.`)
}
