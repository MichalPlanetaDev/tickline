package cli_test

import (
	"bytes"
	"encoding/json"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/cli"
)

func TestWorkflowRunExecutesResolvedPlan(
	t *testing.T,
) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"workflow",
			"run",
			"--plain",
			"release-readiness",
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

	if stderr.String() != "" {
		t.Fatalf(
			"expected empty stderr, got %q",
			stderr.String(),
		)
	}

	output := stdout.String()

	for _, expected := range []string{
		"Tickline local verification",
		"Documentation",
		"C++",
		"Docker",
		"Result: passed",
		"3 passed",
		"Report: reports/check-local/",
	} {
		if !strings.Contains(output, expected) {
			t.Fatalf(
				"expected output to contain %q: %q",
				expected,
				output,
			)
		}
	}
}

func TestWorkflowRunWritesCanonicalJSON(
	t *testing.T,
) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"workflow",
			"run",
			"--json",
			"release-readiness",
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

	if stderr.String() != "" {
		t.Fatalf(
			"expected empty stderr, got %q",
			stderr.String(),
		)
	}

	var document struct {
		SchemaVersion int    `json:"schema_version"`
		Status        string `json:"status"`
		ResultPath    string `json:"result_path"`
		Stages        []struct {
			ID     string `json:"id"`
			Status string `json:"status"`
		} `json:"stages"`
	}

	if err := json.Unmarshal(
		stdout.Bytes(),
		&document,
	); err != nil {
		t.Fatalf(
			"decode workflow result: %v\n%s",
			err,
			stdout.String(),
		)
	}

	if document.SchemaVersion != 1 {
		t.Fatalf(
			"expected schema version 1, got %d",
			document.SchemaVersion,
		)
	}

	if document.Status != "passed" {
		t.Fatalf(
			"expected passed result, got %q",
			document.Status,
		)
	}

	if len(document.Stages) != 3 {
		t.Fatalf(
			"expected 3 stages, got %d",
			len(document.Stages),
		)
	}

	expectedIDs := []string{
		"docs",
		"cpp",
		"docker",
	}

	for index, expectedID := range expectedIDs {
		stage := document.Stages[index]

		if stage.ID != expectedID ||
			stage.Status != "passed" {
			t.Fatalf(
				"unexpected stage %d: %#v",
				index,
				stage,
			)
		}
	}

	if !strings.HasPrefix(
		document.ResultPath,
		"reports/check-local/",
	) {
		t.Fatalf(
			"unexpected result path: %q",
			document.ResultPath,
		)
	}
}

func TestWorkflowRunRejectsUnknownWorkflow(
	t *testing.T,
) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"workflow",
			"run",
			"missing",
		},
		cli.Dependencies{
			Stdout:           &stdout,
			Stderr:           &stderr,
			WorkingDirectory: root,
		},
	)

	if exitCode != cli.ExitInvalidUsage {
		t.Fatalf(
			"expected invalid usage, got %d",
			exitCode,
		)
	}

	if !strings.Contains(
		stderr.String(),
		"unknown workflow",
	) {
		t.Fatalf(
			"unexpected stderr: %q",
			stderr.String(),
		)
	}
}

func TestWorkflowRunRejectsConflictingModes(
	t *testing.T,
) {
	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"workflow",
			"run",
			"--plain",
			"--json",
			"release-readiness",
		},
		cli.Dependencies{
			Stdout: &stdout,
			Stderr: &stderr,
		},
	)

	if exitCode != cli.ExitInvalidUsage {
		t.Fatalf(
			"expected invalid usage, got %d",
			exitCode,
		)
	}

	if !strings.Contains(
		stderr.String(),
		"mutually exclusive",
	) {
		t.Fatalf(
			"unexpected stderr: %q",
			stderr.String(),
		)
	}
}
