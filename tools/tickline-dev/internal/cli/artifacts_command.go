package cli

import (
	"errors"
	"fmt"
	"io"
	"os"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/artifactverify"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/repository"
)

func runArtifacts(
	args []string,
	dependencies Dependencies,
) int {
	if len(args) == 0 {
		printArtifactsHelp(dependencies.Stdout)
		return ExitSuccess
	}

	switch args[0] {
	case "help", "-h", "--help":
		printArtifactsHelp(dependencies.Stdout)
		return ExitSuccess

	case "verify":
		return runArtifactsVerify(
			args[1:],
			dependencies,
		)

	default:
		fmt.Fprintf(
			dependencies.Stderr,
			"unknown artifacts command: %s\n\n",
			args[0],
		)

		printArtifactsHelp(dependencies.Stderr)

		return ExitInvalidUsage
	}
}

func runArtifactsVerify(
	args []string,
	dependencies Dependencies,
) int {
	manifestPath, jsonOutput, help, err :=
		parseArtifactsVerifyArguments(args)

	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"artifacts verify: %v\n",
			err,
		)

		return ExitInvalidUsage
	}

	if help {
		printArtifactsVerifyHelp(dependencies.Stdout)
		return ExitSuccess
	}

	repositoryRoot, err := repository.Find(
		dependencies.WorkingDirectory,
	)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"artifacts verify: resolve repository: %v\n",
			err,
		)

		return ExitInvalidUsage
	}

	report, err := artifactverify.VerifyFile(
		repositoryRoot,
		dependencies.WorkingDirectory,
		manifestPath,
	)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"artifacts verify: %v\n",
			err,
		)

		if errors.Is(
			err,
			artifactverify.ErrManifestOutsideRepository,
		) || errors.Is(err, os.ErrNotExist) {
			return ExitInvalidUsage
		}

		return ExitInternalError
	}

	if jsonOutput {
		if err := writeIndentedJSON(
			dependencies.Stdout,
			report,
		); err != nil {
			fmt.Fprintf(
				dependencies.Stderr,
				"artifacts verify: write JSON: %v\n",
				err,
			)

			return ExitInternalError
		}
	} else {
		printArtifactVerificationReport(
			dependencies.Stdout,
			report,
		)
	}

	if report.Status == artifactverify.StatusPassed {
		return ExitSuccess
	}

	return ExitCheckFailed
}

func parseArtifactsVerifyArguments(
	args []string,
) (string, bool, bool, error) {
	var manifestPath string
	jsonOutput := false

	for _, argument := range args {
		switch argument {
		case "--json":
			if jsonOutput {
				return "", false, false, fmt.Errorf(
					"--json was provided more than once",
				)
			}

			jsonOutput = true

		case "-h", "--help":
			return "", false, true, nil

		default:
			if manifestPath != "" {
				return "", false, false, fmt.Errorf(
					"unexpected argument %q",
					argument,
				)
			}

			manifestPath = argument
		}
	}

	if manifestPath == "" {
		return "", false, false, errors.New(
			"artifact manifest path is required",
		)
	}

	return manifestPath, jsonOutput, false, nil
}

func printArtifactVerificationReport(
	output io.Writer,
	report artifactverify.Report,
) {
	fmt.Fprintln(
		output,
		"Tickline artifact verification",
	)
	fmt.Fprintln(output)

	fmt.Fprintf(
		output,
		"Manifest: %s\n",
		report.ManifestPath,
	)

	if report.RunID != "" {
		fmt.Fprintf(
			output,
			"Run: %s\n",
			report.RunID,
		)
	}

	fmt.Fprintf(
		output,
		"Artifacts: %d\n",
		report.ArtifactCount,
	)

	fmt.Fprintf(
		output,
		"Result: %s\n",
		report.Status,
	)

	if report.Error != "" {
		fmt.Fprintf(
			output,
			"Error: %s\n",
			report.Error,
		)
	}
}

func printArtifactsHelp(output io.Writer) {
	fmt.Fprintln(output, `Tickline artifact operations

Usage:
  tickline-dev artifacts <command>

Commands:
  verify      Verify a run artifact integrity manifest

Verification:
  artifacts verify [--json] <artifacts.json>`)
}

func printArtifactsVerifyHelp(output io.Writer) {
	fmt.Fprintln(output, `Usage:
  tickline-dev artifacts verify [--json] <artifacts.json>

The command validates the manifest schema and recalculates the size and
SHA-256 digest of every declared run artifact.

The manifest must resolve to a regular file inside the current repository.`)
}
