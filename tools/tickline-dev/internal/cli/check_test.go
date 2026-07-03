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

func TestCheckPrintsDefaultExecutionPlan(t *testing.T) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{"check", "--plan"},
		cli.Dependencies{
			Stdout: &stdout,
			Stderr: &stderr,
			WorkingDirectory: filepath.Join(
				root,
				"tools",
				"tickline-dev",
			),
		},
	)

	if exitCode != cli.ExitSuccess {
		t.Fatalf(
			"expected success, got exit code %d",
			exitCode,
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
		"Planning only",
		"docs",
		"cpp",
		"docker",
		"Stages: 3",
	} {
		if !strings.Contains(output, expected) {
			t.Fatalf(
				"expected output to contain %q, got %q",
				expected,
				output,
			)
		}
	}
}

func TestCheckExecutesSelectedStage(t *testing.T) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{"check", "--only", "docs"},
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
		"[1/1] Documentation",
		"documentation fixture",
		"Result: passed",
		"1 passed",
		"Logs: reports/check-local/",
	} {
		if !strings.Contains(output, expected) {
			t.Fatalf(
				"expected output to contain %q, got %q",
				expected,
				output,
			)
		}
	}
}

func TestCheckOnlyIncludesRequiredDependencies(t *testing.T) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"check",
			"--plan",
			"--only",
			"docker",
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

	output := stdout.String()

	docsIndex := strings.Index(output, "docs")
	cppIndex := strings.Index(output, "cpp")
	dockerIndex := strings.Index(output, "docker")

	if docsIndex < 0 ||
		cppIndex < 0 ||
		dockerIndex < 0 {
		t.Fatalf(
			"expected dependency chain in output: %q",
			output,
		)
	}

	if !(docsIndex < cppIndex &&
		cppIndex < dockerIndex) {
		t.Fatalf(
			"unexpected execution order: %q",
			output,
		)
	}
}

func TestCheckRejectsUnknownStage(t *testing.T) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"check",
			"--plan",
			"--only",
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
		"unknown stage",
	) {
		t.Fatalf(
			"unexpected stderr: %q",
			stderr.String(),
		)
	}
}

func TestCheckMapsStageFailureToExitCodeOne(
	t *testing.T,
) {
	root := createCheckRepository(t)

	writeFixtureFile(
		t,
		filepath.Join(
			root,
			"scripts",
			"checks",
			"cpp.sh",
		),
		"#!/usr/bin/env bash\n"+
			"set -Eeuo pipefail\n"+
			"printf 'compiler failure\\n' >&2\n"+
			"exit 7\n",
		0o755,
	)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"check",
			"--only",
			"cpp",
		},
		cli.Dependencies{
			Stdout:           &stdout,
			Stderr:           &stderr,
			WorkingDirectory: root,
		},
	)

	if exitCode != cli.ExitCheckFailed {
		t.Fatalf(
			"expected check failure exit code, got %d",
			exitCode,
		)
	}

	if !strings.Contains(
		stderr.String(),
		"compiler failure",
	) {
		t.Fatalf(
			"expected stage stderr, got %q",
			stderr.String(),
		)
	}

	if !strings.Contains(
		stdout.String(),
		"Result: failed",
	) {
		t.Fatalf(
			"expected failed summary, got %q",
			stdout.String(),
		)
	}
}

func TestCheckWritesVersionedJSON(t *testing.T) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"check",
			"--json",
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

	if stderr.String() != "" {
		t.Fatalf(
			"expected empty stderr, got %q",
			stderr.String(),
		)
	}

	if strings.Contains(
		stdout.String(),
		"documentation fixture",
	) {
		t.Fatalf(
			"stage output leaked into JSON: %q",
			stdout.String(),
		)
	}

	var report struct {
		SchemaVersion int    `json:"schema_version"`
		RunID         string `json:"run_id"`
		Status        string `json:"status"`
		Interrupted   bool   `json:"interrupted"`
		LogDirectory  string `json:"log_directory"`
		Stages        []struct {
			ID       string `json:"id"`
			Status   string `json:"status"`
			ExitCode int    `json:"exit_code"`
			LogPath  string `json:"log_path"`
		} `json:"stages"`
	}

	if err := json.Unmarshal(
		stdout.Bytes(),
		&report,
	); err != nil {
		t.Fatalf(
			"decode JSON result: %v\n%s",
			err,
			stdout.String(),
		)
	}

	if report.SchemaVersion != 1 {
		t.Fatalf(
			"expected schema version 1, got %d",
			report.SchemaVersion,
		)
	}

	if report.RunID == "" {
		t.Fatal("expected non-empty run identifier")
	}

	if report.Status != "passed" {
		t.Fatalf(
			"unexpected status: %q",
			report.Status,
		)
	}

	if report.Interrupted {
		t.Fatal("successful run must not be interrupted")
	}

	if !strings.HasPrefix(
		report.LogDirectory,
		"reports/check-local/",
	) {
		t.Fatalf(
			"unexpected log directory: %q",
			report.LogDirectory,
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
		stage.Status != "passed" ||
		stage.ExitCode != 0 {
		t.Fatalf(
			"unexpected stage result: %#v",
			stage,
		)
	}

	logPath := filepath.Join(
		root,
		filepath.FromSlash(stage.LogPath),
	)

	if _, err := os.Stat(logPath); err != nil {
		t.Fatalf(
			"expected stage log at %q: %v",
			logPath,
			err,
		)
	}
}

func TestCheckRejectsJSONPlanCombination(
	t *testing.T,
) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{
			"check",
			"--json",
			"--plan",
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

	if stdout.String() != "" {
		t.Fatalf(
			"expected empty stdout, got %q",
			stdout.String(),
		)
	}

	if !strings.Contains(
		stderr.String(),
		"cannot be combined",
	) {
		t.Fatalf(
			"unexpected stderr: %q",
			stderr.String(),
		)
	}
}

func createCheckRepository(t *testing.T) string {
	t.Helper()

	root := t.TempDir()

	writeFixtureFile(
		t,
		filepath.Join(root, "CMakeLists.txt"),
		"cmake_minimum_required(VERSION 3.22)\n",
		0o644,
	)

	writeFixtureFile(
		t,
		filepath.Join(
			root,
			"tools",
			"tickline-dev",
			"go.mod",
		),
		"module example.test/tickline-dev\n",
		0o644,
	)

	writeFixtureFile(
		t,
		filepath.Join(
			root,
			"scripts",
			"checks",
			"docs.sh",
		),
		"#!/usr/bin/env bash\n"+
			"set -Eeuo pipefail\n"+
			"printf 'documentation fixture\\n'\n",
		0o755,
	)

	writeFixtureFile(
		t,
		filepath.Join(
			root,
			"scripts",
			"checks",
			"cpp.sh",
		),
		"#!/usr/bin/env bash\n"+
			"set -Eeuo pipefail\n"+
			"printf 'cpp fixture\\n'\n",
		0o755,
	)

	writeFixtureFile(
		t,
		filepath.Join(
			root,
			"scripts",
			"checks",
			"docker.sh",
		),
		"#!/usr/bin/env bash\n"+
			"set -Eeuo pipefail\n"+
			"printf 'docker fixture\\n'\n",
		0o755,
	)

	manifest := strings.Join(
		[]string{
			"version\t1",
			"stage\tdocs\tDocumentation\t" +
				"scripts/checks/docs.sh\ttrue\t",
			"stage\tcpp\tC++\t" +
				"scripts/checks/cpp.sh\ttrue\tdocs",
			"stage\tdocker\tDocker\t" +
				"scripts/checks/docker.sh\ttrue\tcpp",
			"",
		},
		"\n",
	)

	writeFixtureFile(
		t,
		filepath.Join(
			root,
			"scripts",
			"checks",
			"manifest.tsv",
		),
		manifest,
		0o644,
	)

	return root
}

func writeFixtureFile(
	t *testing.T,
	path string,
	content string,
	permissions os.FileMode,
) {
	t.Helper()

	if err := os.MkdirAll(
		filepath.Dir(path),
		0o755,
	); err != nil {
		t.Fatalf(
			"create fixture directory: %v",
			err,
		)
	}

	if err := os.WriteFile(
		path,
		[]byte(content),
		permissions,
	); err != nil {
		t.Fatalf(
			"write fixture file: %v",
			err,
		)
	}
}
