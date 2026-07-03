//go:build linux

package runner_test

import (
	"context"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"testing"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

func TestCancellationTerminatesEntireProcessGroup(
	t *testing.T,
) {
	root := t.TempDir()
	childPIDPath := filepath.Join(root, "child.pid")

	writeScript(
		t,
		root,
		"scripts/checks/process-tree.sh",
		"trap '' TERM\n"+
			"(\n"+
			"    trap '' TERM\n"+
			"    while true; do sleep 1; done\n"+
			") &\n"+
			"printf '%s\\n' \"$!\" > "+
			shellQuote(childPIDPath)+"\n"+
			"printf 'ready\\n'\n"+
			"wait\n",
	)

	ctx, cancel := context.WithCancel(
		context.Background(),
	)
	defer cancel()

	var cancelOnce sync.Once

	executor := runner.Runner{
		RepositoryRoot:         root,
		TerminationGracePeriod: 50 * time.Millisecond,
		Observer: func(event runner.Event) {
			if event.Kind != runner.EventStageOutput {
				return
			}

			if strings.Contains(
				string(event.Data),
				"ready",
			) {
				cancelOnce.Do(cancel)
			}
		},
	}

	result, err := executor.Run(
		ctx,
		[]task.Task{
			{
				ID:         "process-tree",
				Label:      "Process tree",
				ScriptPath: "scripts/checks/process-tree.sh",
			},
		},
	)
	if err != nil {
		t.Fatalf("run process tree: %v", err)
	}

	if result.Status != runner.StatusCancelled {
		t.Fatalf(
			"expected cancelled run, got %q",
			result.Status,
		)
	}

	childPIDBytes, err := os.ReadFile(childPIDPath)
	if err != nil {
		t.Fatalf("read child pid: %v", err)
	}

	childPID, err := strconv.Atoi(
		strings.TrimSpace(string(childPIDBytes)),
	)
	if err != nil {
		t.Fatalf("parse child pid: %v", err)
	}

	deadline := time.Now().Add(time.Second)

	for {
		err := syscall.Kill(childPID, 0)

		if err == syscall.ESRCH {
			break
		}

		if err != nil {
			t.Fatalf(
				"inspect descendant process: %v",
				err,
			)
		}

		if time.Now().After(deadline) {
			t.Fatalf(
				"descendant process %d survived cancellation",
				childPID,
			)
		}

		time.Sleep(10 * time.Millisecond)
	}
}

func shellQuote(value string) string {
	return "'" + strings.ReplaceAll(
		value,
		"'",
		"'\"'\"'",
	) + "'"
}
