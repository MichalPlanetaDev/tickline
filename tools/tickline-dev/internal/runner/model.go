package runner

import "time"

type Status string

const (
	StatusPassed        Status = "passed"
	StatusFailed        Status = "failed"
	StatusSkipped       Status = "skipped"
	StatusCancelled     Status = "cancelled"
	StatusInternalError Status = "internal-error"
)

type Stream string

const (
	StreamStdout Stream = "stdout"
	StreamStderr Stream = "stderr"
)

type EventKind string

const (
	EventRunStarted            EventKind = "run-started"
	EventStageStarted          EventKind = "stage-started"
	EventStageOutput           EventKind = "stage-output"
	EventStageFinished         EventKind = "stage-finished"
	EventStageSkipped          EventKind = "stage-skipped"
	EventCancellationRequested EventKind = "cancellation-requested"
	EventRunFinished           EventKind = "run-finished"
)

type Event struct {
	Kind              EventKind
	StageID           string
	Stream            Stream
	Sequence          uint64
	Data              []byte
	Status            Status
	Duration          time.Duration
	ExitCode          int
	TerminationSignal string
	At                time.Time
}

type Observer func(Event)

type StageResult struct {
	ID                string
	Label             string
	ScriptPath        string
	LogPath           string
	Status            Status
	StartedAt         time.Time
	Duration          time.Duration
	ExitCode          int
	TerminationSignal string
	Stdout            []byte
	Stderr            []byte
	Error             string
}

type RunResult struct {
	RunID        string
	LogDirectory string
	Status       Status
	StartedAt    time.Time
	Duration     time.Duration
	Stages       []StageResult
}
