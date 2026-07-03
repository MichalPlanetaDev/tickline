package cli

import (
	"flag"
	"fmt"
	"path/filepath"
	"strings"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/repository"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

func runCheck(args []string, dependencies Dependencies) int {
	flags := flag.NewFlagSet("check", flag.ContinueOnError)
	flags.SetOutput(dependencies.Stderr)

	var onlyValue string
	var skipValue string

	flags.StringVar(
		&onlyValue,
		"only",
		"",
		"comma-separated stages to select",
	)
	flags.StringVar(
		&skipValue,
		"skip",
		"",
		"comma-separated stages to skip",
	)

	if err := flags.Parse(args); err != nil {
		return ExitInvalidUsage
	}

	if flags.NArg() != 0 {
		fmt.Fprintf(
			dependencies.Stderr,
			"unexpected check argument: %s\n",
			flags.Arg(0),
		)
		return ExitInvalidUsage
	}

	repositoryRoot, err := repository.Find(
		dependencies.WorkingDirectory,
	)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"resolve repository: %v\n",
			err,
		)
		return ExitInvalidUsage
	}

	manifestPath := filepath.Join(
		repositoryRoot,
		"scripts",
		"checks",
		"manifest.tsv",
	)

	manifest, err := task.Load(manifestPath, repositoryRoot)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"load verification tasks: %v\n",
			err,
		)
		return ExitInvalidUsage
	}

	plan, err := manifest.BuildPlan(task.Selection{
		Only: splitStageList(onlyValue),
		Skip: splitStageList(skipValue),
	})
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"build execution plan: %v\n",
			err,
		)
		return ExitInvalidUsage
	}

	fmt.Fprintln(
		dependencies.Stdout,
		"Tickline verification execution plan",
	)
	fmt.Fprintln(
		dependencies.Stdout,
		"Planning only: stages have not been executed.",
	)
	fmt.Fprintln(dependencies.Stdout)

	for index, current := range plan {
		fmt.Fprintf(
			dependencies.Stdout,
			"%d. %-18s %s\n",
			index+1,
			current.ID,
			current.Label,
		)
	}

	fmt.Fprintf(
		dependencies.Stdout,
		"\nStages: %d\n",
		len(plan),
	)

	return ExitSuccess
}

func splitStageList(value string) []string {
	if strings.TrimSpace(value) == "" {
		return nil
	}

	rawValues := strings.Split(value, ",")
	values := make([]string, 0, len(rawValues))

	for _, rawValue := range rawValues {
		values = append(values, strings.TrimSpace(rawValue))
	}

	return values
}
