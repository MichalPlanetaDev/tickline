package runner

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"sync/atomic"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

const defaultTerminationGracePeriod = 2 * time.Second

type Runner struct {
	RepositoryRoot         string
	Observer               Observer
	TerminationGracePeriod time.Duration
}

func (runner Runner) Run(
	ctx context.Context,
	plan []task.Task,
) (RunResult, error) {
	if ctx == nil {
		ctx = context.Background()
	}

	root, err := filepath.Abs(runner.RepositoryRoot)
	if err != nil {
		return RunResult{}, fmt.Errorf(
			"resolve repository root: %w",
			err,
		)
	}

	info, err := os.Stat(root)
	if err != nil {
		return RunResult{}, fmt.Errorf(
			"inspect repository root: %w",
			err,
		)
	}

	if !info.IsDir() {
		return RunResult{}, fmt.Errorf(
			"repository root is not a directory: %s",
			root,
		)
	}

	if len(plan) == 0 {
		return RunResult{}, errors.New("execution plan is empty")
	}

	startedAt := time.Now()

	result := RunResult{
		Status:    StatusPassed,
		StartedAt: startedAt,
		Stages:    make([]StageResult, 0, len(plan)),
	}

	var observerMu sync.Mutex
	var sequence atomic.Uint64

	emit := func(event Event) {
		if runner.Observer == nil {
			return
		}

		observerMu.Lock()
		defer observerMu.Unlock()

		runner.Observer(event)
	}

	emit(Event{
		Kind: EventRunStarted,
		At:   startedAt,
	})

	for index, current := range plan {
		if ctx.Err() != nil {
			cancelled := StageResult{
				ID:         current.ID,
				Label:      current.Label,
				ScriptPath: current.ScriptPath,
				Status:     StatusCancelled,
				ExitCode:   130,
				Error:      ctx.Err().Error(),
			}

			result.Stages = append(
				result.Stages,
				cancelled,
			)

			emit(Event{
				Kind:     EventStageFinished,
				StageID:  current.ID,
				Status:   StatusCancelled,
				ExitCode: 130,
				At:       time.Now(),
			})

			appendSkipped(
				&result,
				plan[index+1:],
				emit,
			)

			result.Status = StatusCancelled
			break
		}

		stage := runner.runStage(
			ctx,
			root,
			current,
			&sequence,
			emit,
		)

		result.Stages = append(result.Stages, stage)

		if stage.Status != StatusPassed {
			result.Status = stage.Status

			appendSkipped(
				&result,
				plan[index+1:],
				emit,
			)

			break
		}
	}

	result.Duration = time.Since(startedAt)

	emit(Event{
		Kind:     EventRunFinished,
		Status:   result.Status,
		Duration: result.Duration,
		At:       time.Now(),
	})

	return result, nil
}

func (runner Runner) runStage(
	ctx context.Context,
	root string,
	current task.Task,
	sequence *atomic.Uint64,
	emit func(Event),
) StageResult {
	startedAt := time.Now()

	result := StageResult{
		ID:         current.ID,
		Label:      current.Label,
		ScriptPath: current.ScriptPath,
		Status:     StatusInternalError,
		StartedAt:  startedAt,
		ExitCode:   -1,
	}

	emit(Event{
		Kind:    EventStageStarted,
		StageID: current.ID,
		At:      startedAt,
	})

	commandPath := filepath.Join(
		root,
		filepath.FromSlash(current.ScriptPath),
	)

	command := exec.Command(commandPath)
	command.Dir = root

	configureProcessTree(command)

	stdoutPipe, err := command.StdoutPipe()
	if err != nil {
		result.Error = fmt.Sprintf(
			"create stdout pipe: %v",
			err,
		)

		result.Duration = time.Since(startedAt)
		emitStageFinished(result, emit)

		return result
	}

	stderrPipe, err := command.StderrPipe()
	if err != nil {
		result.Error = fmt.Sprintf(
			"create stderr pipe: %v",
			err,
		)

		result.Duration = time.Since(startedAt)
		emitStageFinished(result, emit)

		return result
	}

	if err := command.Start(); err != nil {
		result.Error = fmt.Sprintf(
			"start stage command: %v",
			err,
		)

		result.Duration = time.Since(startedAt)
		emitStageFinished(result, emit)

		return result
	}

	var stdout bytes.Buffer
	var stderr bytes.Buffer
	var readers sync.WaitGroup

	readErrors := make(chan error, 2)

	readers.Add(2)

	go captureStream(
		stdoutPipe,
		&stdout,
		StreamStdout,
		current.ID,
		sequence,
		emit,
		readErrors,
		&readers,
	)

	go captureStream(
		stderrPipe,
		&stderr,
		StreamStderr,
		current.ID,
		sequence,
		emit,
		readErrors,
		&readers,
	)

	readersDone := make(chan struct{})

	go func() {
		readers.Wait()
		close(readersDone)
	}()

	processDone := make(chan error, 1)

	go func() {
		processDone <- command.Wait()
	}()

	waitErr, cancelled, supervisionErr :=
		awaitCommand(
			ctx,
			command,
			processDone,
			readersDone,
			runner.terminationGracePeriod(),
			emit,
			current.ID,
		)

	close(readErrors)

	result.Stdout = append(
		[]byte(nil),
		stdout.Bytes()...,
	)

	result.Stderr = append(
		[]byte(nil),
		stderr.Bytes()...,
	)

	result.Duration = time.Since(startedAt)
	result.TerminationSignal = terminationSignal(
		command.ProcessState,
	)

	var readErr error

	for currentReadErr := range readErrors {
		readErr = errors.Join(readErr, currentReadErr)
	}

	switch {
	case cancelled:
		result.Status = StatusCancelled
		result.ExitCode = 130
		result.Error = ctx.Err().Error()

		if supervisionErr != nil {
			result.Error = fmt.Sprintf(
				"%s; terminate process tree: %v",
				result.Error,
				supervisionErr,
			)
		}

	case readErr != nil:
		result.Status = StatusInternalError
		result.Error = readErr.Error()

	case waitErr == nil:
		result.Status = StatusPassed
		result.ExitCode = 0

	default:
		var exitError *exec.ExitError

		if errors.As(waitErr, &exitError) {
			result.Status = StatusFailed
			result.ExitCode = exitError.ExitCode()
			result.Error = waitErr.Error()
		} else {
			result.Status = StatusInternalError
			result.Error = waitErr.Error()
		}
	}

	emitStageFinished(result, emit)

	return result
}

func awaitCommand(
	ctx context.Context,
	command *exec.Cmd,
	processDone <-chan error,
	readersDone <-chan struct{},
	gracePeriod time.Duration,
	emit func(Event),
	stageID string,
) (error, bool, error) {
	var waitErr error

	processChannel := processDone
	readersChannel := readersDone

	for processChannel != nil || readersChannel != nil {
		select {
		case currentWaitErr := <-processChannel:
			waitErr = currentWaitErr
			processChannel = nil

		case <-readersChannel:
			readersChannel = nil

		case <-ctx.Done():
			emit(Event{
				Kind:    EventCancellationRequested,
				StageID: stageID,
				At:      time.Now(),
			})

			return terminateAndDrain(
				command,
				processChannel,
				readersChannel,
				waitErr,
				gracePeriod,
			)
		}
	}

	return waitErr, false, nil
}

func terminateAndDrain(
	command *exec.Cmd,
	processDone <-chan error,
	readersDone <-chan struct{},
	waitErr error,
	gracePeriod time.Duration,
) (error, bool, error) {
	supervisionErr := terminateProcessTree(
		command.Process,
	)

	timer := time.NewTimer(gracePeriod)
	defer stopTimer(timer)

	processChannel := processDone
	readersChannel := readersDone

	for processChannel != nil || readersChannel != nil {
		select {
		case currentWaitErr := <-processChannel:
			waitErr = currentWaitErr
			processChannel = nil

		case <-readersChannel:
			readersChannel = nil

		case <-timer.C:
			supervisionErr = errors.Join(
				supervisionErr,
				killProcessTree(command.Process),
			)

			if processChannel != nil {
				waitErr = <-processChannel
			}

			if readersChannel != nil {
				<-readersChannel
			}

			return waitErr, true, supervisionErr
		}
	}

	return waitErr, true, supervisionErr
}

func stopTimer(timer *time.Timer) {
	if !timer.Stop() {
		select {
		case <-timer.C:
		default:
		}
	}
}

func (runner Runner) terminationGracePeriod() time.Duration {
	if runner.TerminationGracePeriod <= 0 {
		return defaultTerminationGracePeriod
	}

	return runner.TerminationGracePeriod
}

func captureStream(
	reader io.Reader,
	target *bytes.Buffer,
	stream Stream,
	stageID string,
	sequence *atomic.Uint64,
	emit func(Event),
	readErrors chan<- error,
	waitGroup *sync.WaitGroup,
) {
	defer waitGroup.Done()

	buffer := make([]byte, 32*1024)

	for {
		count, err := reader.Read(buffer)

		if count > 0 {
			chunk := append(
				[]byte(nil),
				buffer[:count]...,
			)

			_, _ = target.Write(chunk)

			emit(Event{
				Kind:     EventStageOutput,
				StageID:  stageID,
				Stream:   stream,
				Sequence: sequence.Add(1),
				Data:     chunk,
				At:       time.Now(),
			})
		}

		if err != nil {
			if !errors.Is(err, io.EOF) {
				readErrors <- fmt.Errorf(
					"read %s for stage %q: %w",
					stream,
					stageID,
					err,
				)
			}

			return
		}
	}
}

func appendSkipped(
	result *RunResult,
	remaining []task.Task,
	emit func(Event),
) {
	for _, current := range remaining {
		stage := StageResult{
			ID:         current.ID,
			Label:      current.Label,
			ScriptPath: current.ScriptPath,
			Status:     StatusSkipped,
			ExitCode:   -1,
			Error: "not run after an earlier stage " +
				"stopped execution",
		}

		result.Stages = append(
			result.Stages,
			stage,
		)

		emit(Event{
			Kind:    EventStageSkipped,
			StageID: current.ID,
			Status:  StatusSkipped,
			At:      time.Now(),
		})
	}
}

func emitStageFinished(
	result StageResult,
	emit func(Event),
) {
	emit(Event{
		Kind:              EventStageFinished,
		StageID:           result.ID,
		Status:            result.Status,
		Duration:          result.Duration,
		ExitCode:          result.ExitCode,
		TerminationSignal: result.TerminationSignal,
		At:                time.Now(),
	})
}
