package runner_test

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

func TestRunCapturesOutputAndExitStatus(t *testing.T) {
	root := t.TempDir()

	writeScript(
		t,
		root,
		"scripts/checks/pass.sh",
		"printf 'standard output\\n'\n"+
			"printf 'standard error\\n' >&2\n",
	)

	var events []runner.Event

	executor := runner.Runner{
		RepositoryRoot: root,
		Observer: func(event runner.Event) {
			events = append(events, event)
		},
	}

	result, err := executor.Run(
		context.Background(),
		[]task.Task{
			{
				ID:         "pass",
				Label:      "Passing stage",
				ScriptPath: "scripts/checks/pass.sh",
			},
		},
	)
	if err != nil {
		t.Fatalf("run stage: %v", err)
	}

	if result.Status != runner.StatusPassed {
		t.Fatalf(
			"expected passed run, got %q",
			result.Status,
		)
	}

	if len(result.Stages) != 1 {
		t.Fatalf(
			"expected one stage, got %d",
			len(result.Stages),
		)
	}

	stage := result.Stages[0]

	if stage.Status != runner.StatusPassed ||
		stage.ExitCode != 0 {
		t.Fatalf(
			"unexpected stage result: %#v",
			stage,
		)
	}

	if string(stage.Stdout) != "standard output\n" {
		t.Fatalf(
			"unexpected stdout: %q",
			stage.Stdout,
		)
	}

	if string(stage.Stderr) != "standard error\n" {
		t.Fatalf(
			"unexpected stderr: %q",
			stage.Stderr,
		)
	}

	if stage.Duration <= 0 || result.Duration <= 0 {
		t.Fatal("expected positive durations")
	}

	var stdoutSeen bool
	var stderrSeen bool

	for _, event := range events {
		if event.Kind != runner.EventStageOutput {
			continue
		}

		switch event.Stream {
		case runner.StreamStdout:
			stdoutSeen = true
		case runner.StreamStderr:
			stderrSeen = true
		}
	}

	if !stdoutSeen || !stderrSeen {
		t.Fatalf(
			"expected output events for both streams: %#v",
			events,
		)
	}
}

func TestRunStopsAfterFailureAndSkipsRemainingStages(
	t *testing.T,
) {
	root := t.TempDir()

	writeScript(
		t,
		root,
		"scripts/checks/fail.sh",
		"printf 'failure\\n' >&2\nexit 7\n",
	)

	marker := filepath.Join(root, "later-ran")

	writeScript(
		t,
		root,
		"scripts/checks/later.sh",
		"touch "+marker+"\n",
	)

	executor := runner.Runner{
		RepositoryRoot: root,
	}

	result, err := executor.Run(
		context.Background(),
		[]task.Task{
			{
				ID:         "fail",
				Label:      "Failing stage",
				ScriptPath: "scripts/checks/fail.sh",
			},
			{
				ID:         "later",
				Label:      "Later stage",
				ScriptPath: "scripts/checks/later.sh",
			},
		},
	)
	if err != nil {
		t.Fatalf("run stages: %v", err)
	}

	if result.Status != runner.StatusFailed {
		t.Fatalf(
			"expected failed run, got %q",
			result.Status,
		)
	}

	if len(result.Stages) != 2 {
		t.Fatalf(
			"expected two stage results, got %d",
			len(result.Stages),
		)
	}

	if result.Stages[0].ExitCode != 7 ||
		result.Stages[0].Status != runner.StatusFailed {
		t.Fatalf(
			"unexpected failed stage: %#v",
			result.Stages[0],
		)
	}

	if result.Stages[1].Status != runner.StatusSkipped {
		t.Fatalf(
			"expected skipped stage, got %#v",
			result.Stages[1],
		)
	}

	if _, err := os.Stat(marker); !os.IsNotExist(err) {
		t.Fatalf(
			"later stage unexpectedly ran: %v",
			err,
		)
	}
}

func TestRunReportsContextCancellation(t *testing.T) {
	root := t.TempDir()

	writeScript(
		t,
		root,
		"scripts/checks/wait.sh",
		"exec sleep 30\n",
	)

	ctx, cancel := context.WithTimeout(
		context.Background(),
		50*time.Millisecond,
	)
	defer cancel()

	executor := runner.Runner{
		RepositoryRoot: root,
	}

	result, err := executor.Run(
		ctx,
		[]task.Task{
			{
				ID:         "wait",
				Label:      "Waiting stage",
				ScriptPath: "scripts/checks/wait.sh",
			},
		},
	)
	if err != nil {
		t.Fatalf(
			"run cancelled stage: %v",
			err,
		)
	}

	if result.Status != runner.StatusCancelled {
		t.Fatalf(
			"expected cancelled run, got %q",
			result.Status,
		)
	}

	stage := result.Stages[0]

	if stage.Status != runner.StatusCancelled {
		t.Fatalf(
			"unexpected stage status: %#v",
			stage,
		)
	}

	if stage.ExitCode != 130 {
		t.Fatalf(
			"expected exit code 130, got %d",
			stage.ExitCode,
		)
	}

	if !strings.Contains(
		stage.Error,
		"context deadline exceeded",
	) {
		t.Fatalf(
			"unexpected cancellation error: %q",
			stage.Error,
		)
	}
}

func writeScript(
	t *testing.T,
	root string,
	relativePath string,
	body string,
) {
	t.Helper()

	path := filepath.Join(
		root,
		filepath.FromSlash(relativePath),
	)

	if err := os.MkdirAll(
		filepath.Dir(path),
		0o755,
	); err != nil {
		t.Fatalf(
			"create script directory: %v",
			err,
		)
	}

	content := "#!/usr/bin/env bash\n" +
		"set -Eeuo pipefail\n" +
		body

	if err := os.WriteFile(
		path,
		[]byte(content),
		0o755,
	); err != nil {
		t.Fatalf("write script: %v", err)
	}
}
