package tui

import (
	"strings"
	"testing"
	"time"

	tea "charm.land/bubbletea/v2"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

func TestNewModelCreatesWaitingStages(t *testing.T) {
	model := NewModel(
		[]task.Task{
			{
				ID:    "docs",
				Label: "Documentation",
			},
			{
				ID:    "cpp",
				Label: "C++ build and tests",
			},
		},
		"run-fixture",
	)

	if len(model.stages) != 2 {
		t.Fatalf(
			"expected two stages, got %d",
			len(model.stages),
		)
	}

	for _, current := range model.stages {
		if current.Status != stageWaiting {
			t.Fatalf(
				"expected waiting status, got %q",
				current.Status,
			)
		}
	}

	if model.runID != "run-fixture" {
		t.Fatalf(
			"unexpected run ID: %q",
			model.runID,
		)
	}
}

func TestRunnerEventsUpdateStageState(t *testing.T) {
	model := NewModel(
		[]task.Task{
			{
				ID:    "cpp",
				Label: "C++",
			},
		},
		"run-fixture",
	)

	updated, _ := model.Update(RunnerEventMsg{
		Event: runner.Event{
			Kind:    runner.EventStageStarted,
			StageID: "cpp",
		},
	})

	model = updated.(Model)

	if model.stages[0].Status != stageRunning {
		t.Fatalf(
			"expected running stage, got %q",
			model.stages[0].Status,
		)
	}

	updated, _ = model.Update(RunnerEventMsg{
		Event: runner.Event{
			Kind:     runner.EventStageFinished,
			StageID:  "cpp",
			Status:   runner.StatusPassed,
			Duration: 1250 * time.Millisecond,
			ExitCode: 0,
		},
	})

	model = updated.(Model)

	if model.stages[0].Status != stagePassed {
		t.Fatalf(
			"expected passed stage, got %q",
			model.stages[0].Status,
		)
	}

	if model.stages[0].Duration !=
		1250*time.Millisecond {
		t.Fatalf(
			"unexpected duration: %s",
			model.stages[0].Duration,
		)
	}
}

func TestStageOutputIsSanitized(t *testing.T) {
	model := NewModel(
		[]task.Task{
			{
				ID:    "docs",
				Label: "Documentation",
			},
		},
		"run-fixture",
	)

	updated, _ := model.Update(RunnerEventMsg{
		Event: runner.Event{
			Kind:    runner.EventStageOutput,
			StageID: "docs",
			Stream:  runner.StreamStdout,
			Data: []byte(
				"\x1b[31mfailed\x1b[0m\r\n" +
					"next\tline\x00",
			),
		},
	})

	model = updated.(Model)

	output := model.logs["docs"]

	if strings.Contains(output, "\x1b") {
		t.Fatalf(
			"escape sequence remained in output: %q",
			output,
		)
	}

	if strings.Contains(output, "\x00") {
		t.Fatalf(
			"NUL byte remained in output: %q",
			output,
		)
	}

	if !strings.Contains(output, "failed\nnext    line") {
		t.Fatalf(
			"unexpected sanitized output: %q",
			output,
		)
	}
}

func TestWindowResizeUpdatesLayoutDimensions(
	t *testing.T,
) {
	model := NewModel(nil, "run-fixture")

	updated, _ := model.Update(tea.WindowSizeMsg{
		Width:  140,
		Height: 42,
	})

	model = updated.(Model)

	if model.width != 140 || model.height != 42 {
		t.Fatalf(
			"unexpected dimensions: %dx%d",
			model.width,
			model.height,
		)
	}

	if chooseLayout(model.width) != layoutWide {
		t.Fatal("expected wide layout")
	}
}

func TestSelectionWrapsAtBoundaries(t *testing.T) {
	if previousIndex(0, 3) != 2 {
		t.Fatal("previous selection should wrap")
	}

	if nextIndex(2, 3) != 0 {
		t.Fatal("next selection should wrap")
	}

	if nextIndex(0, 0) != 0 {
		t.Fatal("empty selection should remain zero")
	}
}

func TestViewUsesAlternateScreenAndContainsStages(
	t *testing.T,
) {
	model := NewModel(
		[]task.Task{
			{
				ID:    "docs",
				Label: "Documentation",
			},
			{
				ID:    "cpp",
				Label: "C++ build and tests",
			},
		},
		"run-fixture",
	)

	view := model.View()

	if !view.AltScreen {
		t.Fatal("expected alternate-screen view")
	}

	if view.WindowTitle !=
		"Tickline Developer Console" {
		t.Fatalf(
			"unexpected window title: %q",
			view.WindowTitle,
		)
	}

	if !strings.Contains(
		view.Content,
		"Documentation",
	) {
		t.Fatalf(
			"stage label missing from view: %q",
			view.Content,
		)
	}

	if !strings.Contains(
		view.Content,
		"run-fixture",
	) {
		t.Fatalf(
			"run identifier missing from view: %q",
			view.Content,
		)
	}
}

func TestLayoutBreakpoints(t *testing.T) {
	testCases := []struct {
		width    int
		expected layoutMode
	}{
		{
			width:    50,
			expected: layoutNarrow,
		},
		{
			width:    80,
			expected: layoutMedium,
		},
		{
			width:    140,
			expected: layoutWide,
		},
	}

	for _, testCase := range testCases {
		actual := chooseLayout(testCase.width)

		if actual != testCase.expected {
			t.Fatalf(
				"width %d: expected layout %d, got %d",
				testCase.width,
				testCase.expected,
				actual,
			)
		}
	}
}

func TestCompletedResultUpdatesHeaderState(
	t *testing.T,
) {
	model := NewModel(nil, "initial-run")

	updated, _ := model.Update(RunFinishedMsg{
		Result: runner.RunResult{
			RunID:  "completed-run",
			Status: runner.StatusPassed,
		},
	})

	model = updated.(Model)

	if !model.completed {
		t.Fatal("expected completed model")
	}

	if model.runID != "completed-run" {
		t.Fatalf(
			"unexpected completed run ID: %q",
			model.runID,
		)
	}

	if model.result.Status != runner.StatusPassed {
		t.Fatalf(
			"unexpected result status: %q",
			model.result.Status,
		)
	}
}
