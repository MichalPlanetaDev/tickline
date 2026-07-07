package cli

import (
	"context"
	"errors"
	"fmt"
	"io"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/diagnostics"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/repository"
)

func runDoctor(
	args []string,
	dependencies Dependencies,
) int {
	jsonOutput, help, err := parseDoctorArguments(args)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"doctor: %v\n",
			err,
		)

		return ExitInvalidUsage
	}

	if help {
		printDoctorHelp(dependencies.Stdout)
		return ExitSuccess
	}

	repositoryRoot, err := repository.Find(
		dependencies.WorkingDirectory,
	)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"doctor: resolve repository: %v\n",
			err,
		)

		return ExitInvalidUsage
	}

	report, err := diagnostics.Probe(
		dependencies.Context,
		repositoryRoot,
	)
	if err != nil {
		if errors.Is(err, context.Canceled) ||
			errors.Is(err, context.DeadlineExceeded) {
			fmt.Fprintf(
				dependencies.Stderr,
				"doctor: interrupted: %v\n",
				err,
			)

			return ExitInterrupted
		}

		fmt.Fprintf(
			dependencies.Stderr,
			"doctor: collect diagnostics: %v\n",
			err,
		)

		return ExitInternalError
	}

	if jsonOutput {
		if err := writeIndentedJSON(
			dependencies.Stdout,
			report,
		); err != nil {
			fmt.Fprintf(
				dependencies.Stderr,
				"doctor: write JSON: %v\n",
				err,
			)

			return ExitInternalError
		}
	} else {
		printDoctorReport(
			dependencies.Stdout,
			report,
		)
	}

	if report.Status == diagnostics.StatusPassed {
		return ExitSuccess
	}

	return ExitCheckFailed
}

func parseDoctorArguments(
	args []string,
) (bool, bool, error) {
	jsonOutput := false

	for _, argument := range args {
		switch argument {
		case "--json":
			if jsonOutput {
				return false, false, fmt.Errorf(
					"--json was provided more than once",
				)
			}

			jsonOutput = true

		case "-h", "--help":
			return false, true, nil

		default:
			return false, false, fmt.Errorf(
				"unexpected argument %q",
				argument,
			)
		}
	}

	return jsonOutput, false, nil
}

func printDoctorReport(
	output io.Writer,
	report diagnostics.Report,
) {
	fmt.Fprintln(
		output,
		"Tickline environment diagnostics",
	)
	fmt.Fprintln(output)

	fmt.Fprintf(
		output,
		"Repository: %s\n",
		report.RepositoryRoot,
	)

	platform := report.Platform.OS +
		"/" +
		report.Platform.Arch

	if report.Platform.WSL {
		platform += " (WSL)"
	}

	fmt.Fprintf(
		output,
		"Platform: %s\n\n",
		platform,
	)

	available := 0
	unavailable := 0

	for _, current := range report.Tools {
		marker := string(current.Status)

		if current.Status == diagnostics.ToolAvailable {
			marker = "ok"
			available++
		} else {
			unavailable++
		}

		fmt.Fprintf(
			output,
			"[%s] %-14s %s\n",
			marker,
			current.Label,
			current.Command,
		)

		if current.Path != "" {
			fmt.Fprintf(
				output,
				"  Path: %s\n",
				current.Path,
			)
		}

		if current.Version != "" {
			fmt.Fprintf(
				output,
				"  Version: %s\n",
				current.Version,
			)
		}

		if current.Error != "" {
			fmt.Fprintf(
				output,
				"  Error: %s\n",
				current.Error,
			)
		}
	}

	fmt.Fprintf(
		output,
		"\nResult: %s\n",
		report.Status,
	)

	fmt.Fprintf(
		output,
		"Required tools: %d available, %d unavailable\n",
		available,
		unavailable,
	)
}

func printDoctorHelp(output io.Writer) {
	fmt.Fprintln(output, `Usage:
  tickline-dev doctor [--json]

The command inspects the repository, platform, WSL status, executable paths,
and versions of the required local development tools.

It does not build the project, start containers, or contact remote services.`)
}
