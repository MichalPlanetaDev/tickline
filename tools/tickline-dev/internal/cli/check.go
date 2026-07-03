package cli

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"path/filepath"
	"strings"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/repository"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runlog"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

func runCheck(args []string, dependencies Dependencies) int {
	flags := flag.NewFlagSet(
		"check",
		flag.ContinueOnError,
	)

	flags.SetOutput(dependencies.Stderr)

	var onlyValue string
	var skipValue string
	var planOnly bool

	flags.BoolVar(
		&planOnly,
		"plan",
		false,
		"display the execution plan without running it",
	)

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
		if errors.Is(err, flag.ErrHelp) {
			return ExitSuccess
		}

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

	manifest, err := task.Load(
		manifestPath,
		repositoryRoot,
	)
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

	if planOnly {
		printExecutionPlan(
			dependencies.Stdout,
			plan,
		)

		return ExitSuccess
	}

	logStore, err := runlog.Create(repositoryRoot)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"create verification logs: %v\n",
			err,
		)

		return ExitInternalError
	}

	stageNumbers := make(
		map[string]int,
		len(plan),
	)

	stageLabels := make(
		map[string]string,
		len(plan),
	)

	lastOutputEndedWithNewline := make(
		map[string]bool,
		len(plan),
	)

	for index, current := range plan {
		stageNumbers[current.ID] = index + 1
		stageLabels[current.ID] = current.Label
		lastOutputEndedWithNewline[current.ID] = true
	}

	fmt.Fprintln(
		dependencies.Stdout,
		"Tickline local verification",
	)

	fmt.Fprintln(dependencies.Stdout)

	fmt.Fprintf(
		dependencies.Stdout,
		"Run: %s\n\n",
		logStore.RunID(),
	)

	var logWriteError error

	executor := runner.Runner{
		RepositoryRoot: repositoryRoot,

		Observer: func(event runner.Event) {
			if logWriteError == nil {
				logWriteError = logStore.Observe(event)
			}

			switch event.Kind {
			case runner.EventStageStarted:
				fmt.Fprintf(
					dependencies.Stdout,
					"[%d/%d] %s\n",
					stageNumbers[event.StageID],
					len(plan),
					stageLabels[event.StageID],
				)

			case runner.EventStageOutput:
				output := dependencies.Stdout

				if event.Stream == runner.StreamStderr {
					output = dependencies.Stderr
				}

				_, _ = output.Write(event.Data)

				if len(event.Data) != 0 {
					lastOutputEndedWithNewline[event.StageID] =
						event.Data[len(event.Data)-1] == '\n'
				}

			case runner.EventStageFinished:
				if !lastOutputEndedWithNewline[event.StageID] {
					fmt.Fprintln(dependencies.Stdout)
				}

				printStageCompletion(
					dependencies,
					event,
				)

				fmt.Fprintln(dependencies.Stdout)

			case runner.EventStageSkipped:
				fmt.Fprintf(
					dependencies.Stdout,
					"[%d/%d] %s\n"+
						"    skipped\n\n",
					stageNumbers[event.StageID],
					len(plan),
					stageLabels[event.StageID],
				)
			}
		},
	}

	result, runError := executor.Run(
		dependencies.Context,
		plan,
	)

	closeError := logStore.Close()

	if runError != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"run verification: %v\n",
			errors.Join(runError, closeError),
		)

		return ExitInternalError
	}

	if logWriteError != nil || closeError != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"write verification logs: %v\n",
			errors.Join(logWriteError, closeError),
		)

		return ExitInternalError
	}

	result.RunID = logStore.RunID()
	result.LogDirectory = logStore.RelativeDirectory()

	for index := range result.Stages {
		if result.Stages[index].Status == runner.StatusSkipped {
			continue
		}

		result.Stages[index].LogPath =
			logStore.StageCombinedPath(
				result.Stages[index].ID,
			)
	}

	passed, failed, skipped, cancelled, internal :=
		countStatuses(result.Stages)

	fmt.Fprintf(
		dependencies.Stdout,
		"Result: %s\n",
		result.Status,
	)

	fmt.Fprintf(
		dependencies.Stdout,
		"Checks: %d passed, %d failed, "+
			"%d skipped, %d cancelled, "+
			"%d internal error\n",
		passed,
		failed,
		skipped,
		cancelled,
		internal,
	)

	fmt.Fprintf(
		dependencies.Stdout,
		"Total: %s\n",
		formatDuration(result.Duration),
	)

	fmt.Fprintf(
		dependencies.Stdout,
		"Logs: %s\n",
		result.LogDirectory,
	)

	switch result.Status {
	case runner.StatusPassed:
		return ExitSuccess

	case runner.StatusFailed:
		return ExitCheckFailed

	case runner.StatusCancelled:
		return ExitInterrupted

	case runner.StatusInternalError:
		return ExitInternalError

	default:
		return ExitInternalError
	}
}

func printExecutionPlan(
	output io.Writer,
	plan []task.Task,
) {
	fmt.Fprintln(
		output,
		"Tickline verification execution plan",
	)

	fmt.Fprintln(
		output,
		"Planning only: stages have not been executed.",
	)

	fmt.Fprintln(output)

	for index, current := range plan {
		fmt.Fprintf(
			output,
			"%d. %-18s %s\n",
			index+1,
			current.ID,
			current.Label,
		)
	}

	fmt.Fprintf(
		output,
		"\nStages: %d\n",
		len(plan),
	)
}

func printStageCompletion(
	dependencies Dependencies,
	event runner.Event,
) {
	output := dependencies.Stdout

	if event.Status == runner.StatusFailed ||
		event.Status == runner.StatusInternalError {
		output = dependencies.Stderr
	}

	fmt.Fprintf(
		output,
		"    %s in %s",
		event.Status,
		formatDuration(event.Duration),
	)

	if event.ExitCode >= 0 &&
		event.Status != runner.StatusPassed {
		fmt.Fprintf(
			output,
			" with exit code %d",
			event.ExitCode,
		)
	}

	if event.TerminationSignal != "" {
		fmt.Fprintf(
			output,
			" after signal %s",
			event.TerminationSignal,
		)
	}

	fmt.Fprintln(output)
}

func countStatuses(
	stages []runner.StageResult,
) (int, int, int, int, int) {
	var passed int
	var failed int
	var skipped int
	var cancelled int
	var internal int

	for _, current := range stages {
		switch current.Status {
		case runner.StatusPassed:
			passed++

		case runner.StatusFailed:
			failed++

		case runner.StatusSkipped:
			skipped++

		case runner.StatusCancelled:
			cancelled++

		case runner.StatusInternalError:
			internal++
		}
	}

	return passed, failed, skipped, cancelled, internal
}

func formatDuration(duration time.Duration) string {
	if duration < time.Millisecond {
		return duration.Round(
			time.Microsecond,
		).String()
	}

	return duration.Round(
		time.Millisecond,
	).String()
}

func splitStageList(value string) []string {
	if strings.TrimSpace(value) == "" {
		return nil
	}

	rawValues := strings.Split(value, ",")
	values := make([]string, 0, len(rawValues))

	for _, rawValue := range rawValues {
		values = append(
			values,
			strings.TrimSpace(rawValue),
		)
	}

	return values
}
