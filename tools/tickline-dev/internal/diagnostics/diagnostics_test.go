package diagnostics

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestProbeReportsAvailableToolsInStableOrder(
	t *testing.T,
) {
	directory := createFakeToolchain(t, "")

	t.Setenv("PATH", directory)

	report, err := Probe(
		context.Background(),
		"/repository",
	)
	if err != nil {
		t.Fatalf("probe diagnostics: %v", err)
	}

	if report.SchemaVersion != 1 {
		t.Fatalf(
			"expected schema version 1, got %d",
			report.SchemaVersion,
		)
	}

	if report.Status != StatusPassed {
		t.Fatalf(
			"expected passed status, got %q",
			report.Status,
		)
	}

	expectedIDs := []string{
		"bash",
		"cmake",
		"cpp",
		"python",
		"go",
		"docker",
		"git",
	}

	if len(report.Tools) != len(expectedIDs) {
		t.Fatalf(
			"expected %d tools, got %d",
			len(expectedIDs),
			len(report.Tools),
		)
	}

	for index, expectedID := range expectedIDs {
		current := report.Tools[index]

		if current.ID != expectedID {
			t.Fatalf(
				"tool %d: expected %q, got %q",
				index,
				expectedID,
				current.ID,
			)
		}

		if current.Status != ToolAvailable {
			t.Fatalf(
				"tool %q was not available: %#v",
				current.ID,
				current,
			)
		}

		if current.Path == "" ||
			current.Version == "" {
			t.Fatalf(
				"tool %q has incomplete data: %#v",
				current.ID,
				current,
			)
		}
	}
}

func TestProbeReportsMissingRequiredTool(
	t *testing.T,
) {
	directory := createFakeToolchain(t, "docker")

	t.Setenv("PATH", directory)

	report, err := Probe(
		context.Background(),
		"/repository",
	)
	if err != nil {
		t.Fatalf("probe diagnostics: %v", err)
	}

	if report.Status != StatusFailed {
		t.Fatalf(
			"expected failed status, got %q",
			report.Status,
		)
	}

	docker := findTool(t, report, "docker")

	if docker.Status != ToolMissing {
		t.Fatalf(
			"expected missing Docker result, got %#v",
			docker,
		)
	}

	if docker.Error != "executable not found in PATH" {
		t.Fatalf(
			"unexpected Docker error: %q",
			docker.Error,
		)
	}
}

func createFakeToolchain(
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
				"write fake tool %q: %v",
				command,
				err,
			)
		}
	}

	return directory
}

func findTool(
	t *testing.T,
	report Report,
	identifier string,
) ToolResult {
	t.Helper()

	for _, current := range report.Tools {
		if current.ID == identifier {
			return current
		}
	}

	t.Fatalf("tool %q was not present", identifier)

	return ToolResult{}
}
