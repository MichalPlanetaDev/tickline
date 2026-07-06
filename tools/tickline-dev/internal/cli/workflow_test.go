package cli_test

import (
	"bytes"
	"encoding/json"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/cli"
)

func TestWorkflowListPrintsBuiltInWorkflow(
	t *testing.T,
) {
	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{"workflow", "list"},
		cli.Dependencies{
			Stdout: &stdout,
			Stderr: &stderr,
		},
	)

	if exitCode != cli.ExitSuccess {
		t.Fatalf(
			"expected success, got %d: %s",
			exitCode,
			stderr.String(),
		)
	}

	for _, expected := range []string{
		"Tickline workflows",
		"release-readiness",
		"Release readiness",
		"Workflows: 1",
	} {
		if !strings.Contains(stdout.String(), expected) {
			t.Fatalf(
				"expected output to contain %q: %q",
				expected,
				stdout.String(),
			)
		}
	}
}

func TestWorkflowListWritesVersionedJSON(
	t *testing.T,
) {
	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{"workflow", "list", "--json"},
		cli.Dependencies{
			Stdout: &stdout,
			Stderr: &stderr,
		},
	)

	if exitCode != cli.ExitSuccess {
		t.Fatalf(
			"expected success, got %d: %s",
			exitCode,
			stderr.String(),
		)
	}

	var document struct {
		SchemaVersion int `json:"schema_version"`
		Workflows     []struct {
			ID string `json:"id"`
		} `json:"workflows"`
	}

	if err := json.Unmarshal(
		stdout.Bytes(),
		&document,
	); err != nil {
		t.Fatalf("decode workflow JSON: %v", err)
	}

	if document.SchemaVersion != 1 {
		t.Fatalf(
			"expected schema version 1, got %d",
			document.SchemaVersion,
		)
	}

	if len(document.Workflows) != 1 ||
		document.Workflows[0].ID != "release-readiness" {
		t.Fatalf(
			"unexpected workflows: %#v",
			document.Workflows,
		)
	}
}

func TestWorkflowShowResolvesRepositoryPlan(
	t *testing.T,
) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"workflow",
			"show",
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

	for _, expected := range []string{
		"Workflow: release-readiness",
		"Purpose:",
		"docs",
		"cpp",
		"docker",
		"Artifacts:",
		"reports/check-local/<run-id>/result.json",
	} {
		if !strings.Contains(stdout.String(), expected) {
			t.Fatalf(
				"expected output to contain %q: %q",
				expected,
				stdout.String(),
			)
		}
	}
}

func TestWorkflowShowWritesResolvedJSON(
	t *testing.T,
) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"workflow",
			"show",
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

	var document struct {
		SchemaVersion int `json:"schema_version"`
		Workflow      struct {
			ID     string `json:"id"`
			Stages []struct {
				ID      string `json:"id"`
				Purpose string `json:"purpose"`
			} `json:"stages"`
			Artifacts []struct {
				ID       string `json:"id"`
				Required bool   `json:"required"`
			} `json:"artifacts"`
		} `json:"workflow"`
	}

	if err := json.Unmarshal(
		stdout.Bytes(),
		&document,
	); err != nil {
		t.Fatalf("decode workflow plan: %v", err)
	}

	if document.SchemaVersion != 1 {
		t.Fatalf(
			"expected schema version 1, got %d",
			document.SchemaVersion,
		)
	}

	if document.Workflow.ID != "release-readiness" {
		t.Fatalf(
			"unexpected workflow identifier: %q",
			document.Workflow.ID,
		)
	}

	if len(document.Workflow.Stages) == 0 {
		t.Fatal("expected resolved workflow stages")
	}

	for _, stage := range document.Workflow.Stages {
		if stage.Purpose == "" {
			t.Fatalf(
				"stage %q has no purpose",
				stage.ID,
			)
		}
	}

	if len(document.Workflow.Artifacts) != 3 {
		t.Fatalf(
			"expected 3 artifacts, got %d",
			len(document.Workflow.Artifacts),
		)
	}
}

func TestWorkflowShowRejectsUnknownWorkflow(
	t *testing.T,
) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"workflow",
			"show",
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
