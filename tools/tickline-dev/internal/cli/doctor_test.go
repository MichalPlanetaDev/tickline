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

func TestDoctorWritesVersionedJSON(
	t *testing.T,
) {
	root := createCheckRepository(t)
	t.Setenv(
		"PATH",
		createDoctorToolchain(t, ""),
	)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{"doctor", "--json"},
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

	var report struct {
		SchemaVersion  int    `json:"schema_version"`
		Status         string `json:"status"`
		RepositoryRoot string `json:"repository_root"`
		Tools          []struct {
			ID      string `json:"id"`
			Status  string `json:"status"`
			Version string `json:"version"`
		} `json:"tools"`
	}

	if err := json.Unmarshal(
		stdout.Bytes(),
		&report,
	); err != nil {
		t.Fatalf(
			"decode doctor JSON: %v\n%s",
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

	if report.Status != "passed" {
		t.Fatalf(
			"expected passed status, got %q",
			report.Status,
		)
	}

	if report.RepositoryRoot != root {
		t.Fatalf(
			"expected repository %q, got %q",
			root,
			report.RepositoryRoot,
		)
	}

	if len(report.Tools) != 7 {
		t.Fatalf(
			"expected 7 tools, got %d",
			len(report.Tools),
		)
	}
}

func TestDoctorReturnsFailureForMissingTool(
	t *testing.T,
) {
	root := createCheckRepository(t)

	t.Setenv(
		"PATH",
		createDoctorToolchain(t, "docker"),
	)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{"doctor"},
		cli.Dependencies{
			Stdout:           &stdout,
			Stderr:           &stderr,
			WorkingDirectory: root,
		},
	)

	if exitCode != cli.ExitCheckFailed {
		t.Fatalf(
			"expected check failure, got %d",
			exitCode,
		)
	}

	for _, expected := range []string{
		"[missing] Docker",
		"Result: failed",
		"6 available, 1 unavailable",
	} {
		if !strings.Contains(
			stdout.String(),
			expected,
		) {
			t.Fatalf(
				"expected output to contain %q: %q",
				expected,
				stdout.String(),
			)
		}
	}
}

func createDoctorToolchain(
	t *testing.T,
	omittedCommand string,
) string {
	t.Helper()

	directory := t.TempDir()

	for _, command := range []string{
		"bash",
		"cmake",
		"c++",
		"python3",
		"go",
		"docker",
		"git",
	} {
		if command == omittedCommand {
			continue
		}

		content := "#!/bin/sh\nprintf '" +
			command +
			" version 1.0\\n'\n"

		path := filepath.Join(directory, command)

		if err := os.WriteFile(
			path,
			[]byte(content),
			0o755,
		); err != nil {
			t.Fatalf(
				"write fake command %q: %v",
				command,
				err,
			)
		}
	}

	return directory
}
