package cli

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"path/filepath"
	"strings"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/report/jsonreport"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/repository"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runlog"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/tui"
)

const resultArtifactName = "result.json"

type checkOutputMode int

const (
	checkOutputPlain checkOutputMode = iota
	checkOutputJSON
	checkOutputTUI
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
	var plainOutput bool
	var jsonOutput bool
	var tuiOutput bool

	flags.BoolVar(
		&planOnly,
		"plan",
		false,
		"display the execution plan without running it",
	)

	flags.BoolVar(
		&plainOutput,
		"plain",
		false,
		"force line-oriented plain output",
	)

	flags.BoolVar(
		&jsonOutput,
		"json",
		false,
		"emit one versioned JSON result document",
	)

	flags.BoolVar(
		&tuiOutput,
		"tui",
		false,
		"force the interactive terminal interface",
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

	mode, err := resolveCheckOutputMode(
		planOnly,
		plainOutput,
		jsonOutput,
		tuiOutput,
		dependencies.TerminalAvailable,
	)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"select output mode: %v\n",
			err,
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

	execute := newExecutionFunction(
		repositoryRoot,
		plan,
		logStore,
	)

	var result runner.RunResult

	switch mode {
	case checkOutputTUI:
		if dependencies.Stdin == nil {
			fmt.Fprintln(
				dependencies.Stderr,
				"interactive mode requires an input stream",
			)

			_ = logStore.Close()

			return ExitInternalError
		}

		result, err = tui.Run(
			dependencies.Context,
			dependencies.Stdin,
			dependencies.Stdout,
			plan,
			logStore.RunID(),
			execute,
		)

	case checkOutputJSON:
		result, err = execute(
			dependencies.Context,
			nil,
		)

	case checkOutputPlain:
		printPlainHeader(
			dependencies.Stdout,
			logStore.RunID(),
		)

		result, err = execute(
			dependencies.Context,
			newPlainObserver(
				dependencies,
				plan,
			),
		)

	default:
		err = errors.Join(
			errors.New("unsupported output mode"),
			logStore.Close(),
		)
	}

	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"run verification: %v\n",
			err,
		)

		return ExitInternalError
	}

	switch mode {
	case checkOutputJSON:
		if err := jsonreport.Write(
			dependencies.Stdout,
			result,
		); err != nil {
			fmt.Fprintf(
				dependencies.Stderr,
				"write JSON result: %v\n",
				err,
			)

			return ExitInternalError
		}

	case checkOutputPlain:
		printPlainSummary(
			dependencies.Stdout,
			result,
		)
	}

	return exitCodeForStatus(result.Status)
}

func resolveCheckOutputMode(
	planOnly bool,
	plainOutput bool,
	jsonOutput bool,
	tuiOutput bool,
	terminalAvailable bool,
) (checkOutputMode, error) {
	explicitModes := 0

	for _, enabled := range []bool{
		plainOutput,
		jsonOutput,
		tuiOutput,
	} {
		if enabled {
			explicitModes++
		}
	}

	if explicitModes > 1 {
		return checkOutputPlain, errors.New(
			"--plain, --json, and --tui are mutually exclusive",
		)
	}

	if planOnly && explicitModes != 0 {
		return checkOutputPlain, errors.New(
			"--plan cannot be combined with an output-mode flag",
		)
	}

	if jsonOutput {
		return checkOutputJSON, nil
	}

	if plainOutput {
		return checkOutputPlain, nil
	}

	if tuiOutput {
		if !terminalAvailable {
			return checkOutputPlain, errors.New(
				"--tui requires an interactive input and output terminal",
			)
		}

		return checkOutputTUI, nil
	}

	if terminalAvailable {
		return checkOutputTUI, nil
	}

	return checkOutputPlain, nil
}

func newExecutionFunction(
	repositoryRoot string,
	plan []task.Task,
	logStore *runlog.Store,
) tui.ExecuteFunc {
	return func(
		ctx context.Context,
		observer runner.Observer,
	) (runner.RunResult, error) {
		var logWriteError error

		executor := runner.Runner{
			RepositoryRoot: repositoryRoot,

			Observer: func(event runner.Event) {
				if logWriteError == nil {
					logWriteError = logStore.Observe(event)
				}

				if observer != nil {
					observer(event)
				}
			},
		}

		result, runError := executor.Run(
			ctx,
			plan,
		)

		result.RunID = logStore.RunID()
		result.LogDirectory = logStore.RelativeDirectory()
		result.ResultPath = logStore.ArtifactPath(
			resultArtifactName,
		)

		for index := range result.Stages {
			if result.Stages[index].StartedAt.IsZero() {
				continue
			}

			result.Stages[index].LogPath =
				logStore.StageCombinedPath(
					result.Stages[index].ID,
				)
		}

		reportData, reportError :=
			jsonreport.Marshal(result)

		var artifactError error

		if reportError == nil {
			_, artifactError = logStore.WriteArtifact(
				resultArtifactName,
				reportData,
			)
		}

		closeError := logStore.Close()

		return result, errors.Join(
			runError,
			logWriteError,
			reportError,
			artifactError,
			closeError,
		)
	}
}

func printPlainHeader(
	output io.Writer,
	runID string,
) {
	fmt.Fprintln(
		output,
		"Tickline local verification",
	)

	fmt.Fprintln(output)

	fmt.Fprintf(
		output,
		"Run: %s\n\n",
		runID,
	)
}

func newPlainObserver(
	dependencies Dependencies,
	plan []task.Task,
) runner.Observer {
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

	return func(event runner.Event) {
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
	}
}

func printPlainSummary(
	output io.Writer,
	result runner.RunResult,
) {
	passed, failed, skipped, cancelled, internal :=
		countStatuses(result.Stages)

	fmt.Fprintf(
		output,
		"Result: %s\n",
		result.Status,
	)

	fmt.Fprintf(
		output,
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
		output,
		"Total: %s\n",
		formatDuration(result.Duration),
	)

	fmt.Fprintf(
		output,
		"Logs: %s\n",
		result.LogDirectory,
	)

	fmt.Fprintf(
		output,
		"Report: %s\n",
		result.ResultPath,
	)
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

func exitCodeForStatus(status runner.Status) int {
	switch status {
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
