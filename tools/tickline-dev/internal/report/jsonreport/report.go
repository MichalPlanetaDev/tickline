package jsonreport

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
)

const SchemaVersion = 1

type document struct {
	SchemaVersion int             `json:"schema_version"`
	RunID         string          `json:"run_id"`
	Status        runner.Status   `json:"status"`
	StartedAt     string          `json:"started_at"`
	DurationMS    int64           `json:"duration_ms"`
	Interrupted   bool            `json:"interrupted"`
	LogDirectory  string          `json:"log_directory"`
	Stages        []stageDocument `json:"stages"`
}

type stageDocument struct {
	ID                string        `json:"id"`
	Label             string        `json:"label"`
	Status            runner.Status `json:"status"`
	StartedAt         string        `json:"started_at,omitempty"`
	DurationMS        int64         `json:"duration_ms"`
	ExitCode          int           `json:"exit_code"`
	TerminationSignal string        `json:"termination_signal,omitempty"`
	ScriptPath        string        `json:"script_path"`
	LogPath           string        `json:"log_path,omitempty"`
	Error             string        `json:"error,omitempty"`
}

func Write(
	writer io.Writer,
	result runner.RunResult,
) error {
	data, err := Marshal(result)
	if err != nil {
		return err
	}

	if _, err := io.Copy(
		writer,
		bytes.NewReader(data),
	); err != nil {
		return fmt.Errorf("write JSON report: %w", err)
	}

	return nil
}

func Marshal(
	result runner.RunResult,
) ([]byte, error) {
	stages := make(
		[]stageDocument,
		0,
		len(result.Stages),
	)

	for _, current := range result.Stages {
		stages = append(
			stages,
			stageDocument{
				ID:                current.ID,
				Label:             current.Label,
				Status:            current.Status,
				StartedAt:         formatTime(current.StartedAt),
				DurationMS:        current.Duration.Milliseconds(),
				ExitCode:          current.ExitCode,
				TerminationSignal: current.TerminationSignal,
				ScriptPath:        current.ScriptPath,
				LogPath:           current.LogPath,
				Error:             current.Error,
			},
		)
	}

	report := document{
		SchemaVersion: SchemaVersion,
		RunID:         result.RunID,
		Status:        result.Status,
		StartedAt:     formatTime(result.StartedAt),
		DurationMS:    result.Duration.Milliseconds(),
		Interrupted:   result.Status == runner.StatusCancelled,
		LogDirectory:  result.LogDirectory,
		Stages:        stages,
	}

	data, err := json.MarshalIndent(
		report,
		"",
		"  ",
	)
	if err != nil {
		return nil, fmt.Errorf(
			"encode JSON report: %w",
			err,
		)
	}

	return append(data, '\n'), nil
}

func formatTime(value time.Time) string {
	if value.IsZero() {
		return ""
	}

	return value.UTC().Format(time.RFC3339Nano)
}
