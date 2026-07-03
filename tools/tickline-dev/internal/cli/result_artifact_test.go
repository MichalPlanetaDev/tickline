package cli_test

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/cli"
)

func TestCheckPersistsCanonicalResultDocument(
	t *testing.T,
) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"check",
			"--plain",
			"--only",
			"docs",
		},
		cli.Dependencies{
			Stdout:           &stdout,
			Stderr:           &stderr,
			WorkingDirectory: root,
		},
	)

	if exitCode != cli.ExitSuccess {
		t.Fatalf(
			"expected success, got %d: %s",
			exitCode,
			stderr.String(),
		)
	}

	matches, err := filepath.Glob(
		filepath.Join(
			root,
			"reports",
			"check-local",
			"*",
			"result.json",
		),
	)
	if err != nil {
		t.Fatalf("search result documents: %v", err)
	}

	if len(matches) != 1 {
		t.Fatalf(
			"expected one result document, got %d: %#v",
			len(matches),
			matches,
		)
	}

	data, err := os.ReadFile(matches[0])
	if err != nil {
		t.Fatalf("read result document: %v", err)
	}

	var report struct {
		SchemaVersion int    `json:"schema_version"`
		RunID         string `json:"run_id"`
		Status        string `json:"status"`
		LogDirectory  string `json:"log_directory"`
		ResultPath    string `json:"result_path"`
		Stages        []struct {
			ID      string `json:"id"`
			Status  string `json:"status"`
			LogPath string `json:"log_path"`
		} `json:"stages"`
	}

	if err := json.Unmarshal(
		data,
		&report,
	); err != nil {
		t.Fatalf(
			"decode result document: %v",
			err,
		)
	}

	if report.SchemaVersion != 1 {
		t.Fatalf(
			"expected schema version 1, got %d",
			report.SchemaVersion,
		)
	}

	if report.RunID == "" {
		t.Fatal("expected a run identifier")
	}

	if report.Status != "passed" {
		t.Fatalf(
			"expected passed status, got %q",
			report.Status,
		)
	}

	expectedDirectory :=
		"reports/check-local/" + report.RunID

	if report.LogDirectory != expectedDirectory {
		t.Fatalf(
			"expected log directory %q, got %q",
			expectedDirectory,
			report.LogDirectory,
		)
	}

	expectedResultPath :=
		expectedDirectory + "/result.json"

	if report.ResultPath != expectedResultPath {
		t.Fatalf(
			"expected result path %q, got %q",
			expectedResultPath,
			report.ResultPath,
		)
	}

	if len(report.Stages) != 1 {
		t.Fatalf(
			"expected one stage, got %d",
			len(report.Stages),
		)
	}

	stage := report.Stages[0]

	if stage.ID != "docs" ||
		stage.Status != "passed" {
		t.Fatalf(
			"unexpected stage result: %#v",
			stage,
		)
	}

	if !strings.HasSuffix(
		stage.LogPath,
		"/docs.combined.log",
	) {
		t.Fatalf(
			"unexpected stage log path: %q",
			stage.LogPath,
		)
	}

	if !strings.Contains(
		stdout.String(),
		"Report: "+expectedResultPath,
	) {
		t.Fatalf(
			"plain output did not expose report path: %q",
			stdout.String(),
		)
	}
}
