package cli_test

import (
	"bytes"
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
		[]string{"check"},
		cli.Dependencies{
			Stdout:           &stdout,
			Stderr:           &stderr,
			WorkingDirectory: filepath.Join(root, "tools", "tickline-dev"),
		},
	)

	if exitCode != cli.ExitSuccess {
		t.Fatalf("expected success, got exit code %d", exitCode)
	}

	if stderr.String() != "" {
		t.Fatalf("expected empty stderr, got %q", stderr.String())
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

func TestCheckOnlyIncludesRequiredDependencies(t *testing.T) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{"check", "--only", "docker"},
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

	if docsIndex < 0 || cppIndex < 0 || dockerIndex < 0 {
		t.Fatalf("expected dependency chain in output: %q", output)
	}

	if !(docsIndex < cppIndex && cppIndex < dockerIndex) {
		t.Fatalf("unexpected execution order: %q", output)
	}
}

func TestCheckRejectsUnknownStage(t *testing.T) {
	root := createCheckRepository(t)

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		[]string{"check", "--only", "missing"},
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

	if !strings.Contains(stderr.String(), "unknown stage") {
		t.Fatalf("unexpected stderr: %q", stderr.String())
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
		filepath.Join(root, "tools", "tickline-dev", "go.mod"),
		"module example.test/tickline-dev\n",
		0o644,
	)

	scripts := []string{
		"docs.sh",
		"cpp.sh",
		"docker.sh",
	}

	for _, script := range scripts {
		writeFixtureFile(
			t,
			filepath.Join(root, "scripts", "checks", script),
			"#!/usr/bin/env bash\nexit 0\n",
			0o755,
		)
	}

	manifest := strings.Join([]string{
		"version\t1",
		"stage\tdocs\tDocumentation\tscripts/checks/docs.sh\ttrue\t",
		"stage\tcpp\tC++\tscripts/checks/cpp.sh\ttrue\tdocs",
		"stage\tdocker\tDocker\tscripts/checks/docker.sh\ttrue\tcpp",
		"",
	}, "\n")

	writeFixtureFile(
		t,
		filepath.Join(root, "scripts", "checks", "manifest.tsv"),
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

	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatalf("create fixture directory: %v", err)
	}

	if err := os.WriteFile(
		path,
		[]byte(content),
		permissions,
	); err != nil {
		t.Fatalf("write fixture file: %v", err)
	}
}
