package tui

import (
	"context"
	"errors"
	"fmt"
	"io"

	tea "charm.land/bubbletea/v2"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

type ExecuteFunc func(
	context.Context,
	runner.Observer,
) (runner.RunResult, error)

func Run(
	parentContext context.Context,
	input io.Reader,
	output io.Writer,
	plan []task.Task,
	runID string,
	execute ExecuteFunc,
) (runner.RunResult, error) {
	if parentContext == nil {
		parentContext = context.Background()
	}

	if input == nil {
		return runner.RunResult{}, errors.New(
			"terminal input is nil",
		)
	}

	if output == nil {
		return runner.RunResult{}, errors.New(
			"terminal output is nil",
		)
	}

	if execute == nil {
		return runner.RunResult{}, errors.New(
			"execution function is nil",
		)
	}

	runContext, cancel := context.WithCancel(
		parentContext,
	)
	defer cancel()

	events := make(chan tea.Msg, 256)
	interfaceDone := make(chan struct{})
	executionDone := make(chan struct{})

	var executionResult runner.RunResult
	var executionError error

	model := NewModel(
		plan,
		runID,
	).withRuntime(
		events,
		cancel,
	)

	program := tea.NewProgram(
		model,
		tea.WithInput(input),
		tea.WithOutput(output),
	)

	sendMessage := func(message tea.Msg) bool {
		select {
		case events <- message:
			return true

		case <-interfaceDone:
			return false
		}
	}

	go func() {
		defer close(executionDone)

		executionResult, executionError = execute(
			runContext,
			func(event runner.Event) {
				sendMessage(RunnerEventMsg{
					Event: event,
				})
			},
		)

		sendMessage(RunFinishedMsg{
			Result: executionResult,
			Err:    executionError,
		})
	}()

	finalModel, interfaceError := program.Run()
	close(interfaceDone)

	if interfaceError != nil {
		cancel()
	}

	<-executionDone

	if interfaceError != nil {
		return executionResult, fmt.Errorf(
			"run terminal interface: %w",
			interfaceError,
		)
	}

	completedModel, ok := finalModel.(Model)
	if !ok {
		return executionResult, errors.New(
			"terminal interface returned an unexpected model",
		)
	}

	return completedModel.result, completedModel.runError
}

func waitForMessage(
	events <-chan tea.Msg,
) tea.Cmd {
	return func() tea.Msg {
		return <-events
	}
}
