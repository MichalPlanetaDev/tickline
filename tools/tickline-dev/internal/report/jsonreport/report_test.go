package jsonreport_test

import (
	"bytes"
	"encoding/json"
	"strings"
	"testing"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/report/jsonreport"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
)

func TestMarshalCreatesVersionedReport(t *testing.T) {
	startedAt := time.Date(
		2026,
		time.July,
		3,
		15,
		45,
		12,
		123456789,
		time.UTC,
	)

	result := runner.RunResult{
		RunID:        "run-fixture",
		LogDirectory: "reports/check-local/run-fixture",
		Status:       runner.StatusPassed,
		StartedAt:    startedAt,
		Duration:     1500 * time.Millisecond,
		Stages: []runner.StageResult{
			{
				ID:         "docs",
				Label:      "Documentation",
				ScriptPath: "scripts/checks/docs.sh",
				LogPath: "reports/check-local/" +
					"run-fixture/docs.combined.log",
				Status:    runner.StatusPassed,
				StartedAt: startedAt,
				Duration:  250 * time.Millisecond,
				ExitCode:  0,
				Stdout:    []byte("must not be embedded"),
				Stderr:    []byte("must not be embedded"),
			},
		},
	}

	data, err := jsonreport.Marshal(result)
	if err != nil {
		t.Fatalf("marshal report: %v", err)
	}

	if !bytes.HasSuffix(data, []byte{'\n'}) {
		t.Fatal("expected JSON report to end with a newline")
	}

	if bytes.Contains(data, []byte("must not be embedded")) {
		t.Fatal("stage output must not be embedded in JSON")
	}

	var decoded struct {
		SchemaVersion int    `json:"schema_version"`
		RunID         string `json:"run_id"`
		Status        string `json:"status"`
		StartedAt     string `json:"started_at"`
		DurationMS    int64  `json:"duration_ms"`
		Interrupted   bool   `json:"interrupted"`
		LogDirectory  string `json:"log_directory"`
		Stages        []struct {
			ID         string `json:"id"`
			Status     string `json:"status"`
			DurationMS int64  `json:"duration_ms"`
			ExitCode   int    `json:"exit_code"`
			LogPath    string `json:"log_path"`
		} `json:"stages"`
	}

	if err := json.Unmarshal(data, &decoded); err != nil {
		t.Fatalf("decode report: %v", err)
	}

	if decoded.SchemaVersion != jsonreport.SchemaVersion {
		t.Fatalf(
			"expected schema version %d, got %d",
			jsonreport.SchemaVersion,
			decoded.SchemaVersion,
		)
	}

	if decoded.RunID != "run-fixture" {
		t.Fatalf(
			"unexpected run identifier: %q",
			decoded.RunID,
		)
	}

	if decoded.Status != "passed" {
		t.Fatalf(
			"unexpected run status: %q",
			decoded.Status,
		)
	}

	if decoded.StartedAt !=
		"2026-07-03T15:45:12.123456789Z" {
		t.Fatalf(
			"unexpected start time: %q",
			decoded.StartedAt,
		)
	}

	if decoded.DurationMS != 1500 {
		t.Fatalf(
			"expected duration 1500 ms, got %d",
			decoded.DurationMS,
		)
	}

	if decoded.Interrupted {
		t.Fatal("passed run must not be interrupted")
	}

	if decoded.LogDirectory !=
		"reports/check-local/run-fixture" {
		t.Fatalf(
			"unexpected log directory: %q",
			decoded.LogDirectory,
		)
	}

	if len(decoded.Stages) != 1 {
		t.Fatalf(
			"expected one stage, got %d",
			len(decoded.Stages),
		)
	}

	stage := decoded.Stages[0]

	if stage.ID != "docs" ||
		stage.Status != "passed" ||
		stage.DurationMS != 250 ||
		stage.ExitCode != 0 {
		t.Fatalf(
			"unexpected stage report: %#v",
			stage,
		)
	}

	if !strings.HasSuffix(
		stage.LogPath,
		"docs.combined.log",
	) {
		t.Fatalf(
			"unexpected stage log path: %q",
			stage.LogPath,
		)
	}
}

func TestMarshalMarksCancelledRunInterrupted(
	t *testing.T,
) {
	data, err := jsonreport.Marshal(
		runner.RunResult{
			RunID:  "cancelled-run",
			Status: runner.StatusCancelled,
		},
	)
	if err != nil {
		t.Fatalf("marshal report: %v", err)
	}

	var decoded struct {
		Status      string `json:"status"`
		Interrupted bool   `json:"interrupted"`
	}

	if err := json.Unmarshal(data, &decoded); err != nil {
		t.Fatalf("decode report: %v", err)
	}

	if decoded.Status != "cancelled" {
		t.Fatalf(
			"unexpected status: %q",
			decoded.Status,
		)
	}

	if !decoded.Interrupted {
		t.Fatal("cancelled run must be marked interrupted")
	}
}

func TestWriteProducesValidJSON(t *testing.T) {
	var output bytes.Buffer

	err := jsonreport.Write(
		&output,
		runner.RunResult{
			RunID:  "write-fixture",
			Status: runner.StatusPassed,
		},
	)
	if err != nil {
		t.Fatalf("write report: %v", err)
	}

	if !json.Valid(output.Bytes()) {
		t.Fatalf(
			"report is not valid JSON: %q",
			output.String(),
		)
	}
}
