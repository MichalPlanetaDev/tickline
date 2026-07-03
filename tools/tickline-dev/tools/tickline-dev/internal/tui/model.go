package tui

import (
	"fmt"
	"time"

	tea "charm.land/bubbletea/v2"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

const maxStageLogBytes = 256 * 1024

type stageStatus string

const (
	stageWaiting       stageStatus = "waiting"
	stageRunning       stageStatus = "running"
	stagePassed        stageStatus = "passed"
	stageFailed        stageStatus = "failed"
	stageSkipped       stageStatus = "skipped"
	stageCancelled     stageStatus = "cancelled"
	stageInternalError stageStatus = "internal-error"
)

type stageState struct {
	ID                string
	Label             string
	Status            stageStatus
	Duration          time.Duration
	ExitCode          int
	TerminationSignal string
}

type RunnerEventMsg struct {
	Event runner.Event
}

type RunFinishedMsg struct {
	Result runner.RunResult
	Err    error
}

type Model struct {
	runID        string
	stages       []stageState
	stageIndex   map[string]int
	logs         map[string]string
	logTruncated map[string]bool
	selected     int
	width        int
	height       int
	completed    bool
	cancelling   bool
	result       runner.RunResult
	runError     error
}

func NewModel(
	plan []task.Task,
	runID string,
) Model {
	stages := make([]stageState, 0, len(plan))
	stageIndex := make(map[string]int, len(plan))
	logs := make(map[string]string, len(plan))
	logTruncated := make(map[string]bool, len(plan))

	for index, current := range plan {
		stages = append(stages, stageState{
			ID:       current.ID,
			Label:    current.Label,
			Status:   stageWaiting,
			ExitCode: -1,
		})

		stageIndex[current.ID] = index
		logs[current.ID] = ""
		logTruncated[current.ID] = false
	}

	return Model{
		runID:        runID,
		stages:       stages,
		stageIndex:   stageIndex,
		logs:         logs,
		logTruncated: logTruncated,
		width:        100,
		height:       30,
	}
}

func (model Model) Init() tea.Cmd {
	return nil
}

func (model Model) Update(
	message tea.Msg,
) (tea.Model, tea.Cmd) {
	switch current := message.(type) {
	case tea.WindowSizeMsg:
		model.width = max(current.Width, 1)
		model.height = max(current.Height, 1)

	case tea.KeyPressMsg:
		switch current.String() {
		case "up", "k":
			model.selected = previousIndex(
				model.selected,
				len(model.stages),
			)

		case "down", "j":
			model.selected = nextIndex(
				model.selected,
				len(model.stages),
			)

		case "home", "g":
			if len(model.stages) != 0 {
				model.selected = 0
			}

		case "end", "G":
			if len(model.stages) != 0 {
				model.selected = len(model.stages) - 1
			}

		case "q", "esc":
			return model, tea.Quit

		case "ctrl+c":
			model.cancelling = true
		}

	case RunnerEventMsg:
		model.applyRunnerEvent(current.Event)

	case RunFinishedMsg:
		model.completed = true
		model.result = current.Result
		model.runError = current.Err

		if current.Result.RunID != "" {
			model.runID = current.Result.RunID
		}
	}

	return model, nil
}

func (model Model) View() tea.View {
	content := model.render()

	view := tea.NewView(content)
	view.AltScreen = true
	view.WindowTitle = "Tickline Developer Console"

	return view
}

func (model *Model) applyRunnerEvent(
	event runner.Event,
) {
	index, exists := model.stageIndex[event.StageID]

	switch event.Kind {
	case runner.EventStageStarted:
		if !exists {
			return
		}

		model.stages[index].Status = stageRunning
		model.selected = index

	case runner.EventStageOutput:
		if !exists {
			return
		}

		model.appendStageOutput(
			event.StageID,
			event.Data,
		)

	case runner.EventStageFinished:
		if !exists {
			return
		}

		model.stages[index].Status =
			statusFromRunner(event.Status)

		model.stages[index].Duration = event.Duration
		model.stages[index].ExitCode = event.ExitCode
		model.stages[index].TerminationSignal =
			event.TerminationSignal

	case runner.EventStageSkipped:
		if !exists {
			return
		}

		model.stages[index].Status = stageSkipped

	case runner.EventCancellationRequested:
		model.cancelling = true
	}
}

func (model *Model) appendStageOutput(
	stageID string,
	data []byte,
) {
	sanitized := sanitizeTerminalOutput(data)
	if sanitized == "" {
		return
	}

	combined := model.logs[stageID] + sanitized

	if len(combined) > maxStageLogBytes {
		combined = combined[len(combined)-maxStageLogBytes:]

		combined = string([]rune(
			fmt.Sprintf("%s", combined),
		))

		model.logTruncated[stageID] = true
	}

	model.logs[stageID] = combined
}

func statusFromRunner(
	status runner.Status,
) stageStatus {
	switch status {
	case runner.StatusPassed:
		return stagePassed

	case runner.StatusFailed:
		return stageFailed

	case runner.StatusSkipped:
		return stageSkipped

	case runner.StatusCancelled:
		return stageCancelled

	case runner.StatusInternalError:
		return stageInternalError

	default:
		return stageInternalError
	}
}

func previousIndex(
	current int,
	length int,
) int {
	if length == 0 {
		return 0
	}

	if current <= 0 {
		return length - 1
	}

	return current - 1
}

func nextIndex(
	current int,
	length int,
) int {
	if length == 0 {
		return 0
	}

	if current >= length-1 {
		return 0
	}

	return current + 1
}
