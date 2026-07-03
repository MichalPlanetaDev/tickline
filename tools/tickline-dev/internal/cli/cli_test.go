package cli_test

import (
	"bytes"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/cli"
)

func runCommand(t *testing.T, args ...string) (int, string, string) {
	t.Helper()

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		args,
		cli.Dependencies{
			Stdout: &stdout,
			Stderr: &stderr,
		},
	)

	return exitCode, stdout.String(), stderr.String()
}

func TestHelpCommand(t *testing.T) {
	exitCode, stdout, stderr := runCommand(t, "help")

	if exitCode != cli.ExitSuccess {
		t.Fatalf("expected exit code %d, got %d", cli.ExitSuccess, exitCode)
	}

	if !strings.Contains(stdout, "Tickline developer console") {
		t.Fatalf("help output did not contain the application title: %q", stdout)
	}

	if stderr != "" {
		t.Fatalf("expected empty stderr, got %q", stderr)
	}
}

func TestVersionCommand(t *testing.T) {
	exitCode, stdout, stderr := runCommand(t, "version")

	if exitCode != cli.ExitSuccess {
		t.Fatalf("expected exit code %d, got %d", cli.ExitSuccess, exitCode)
	}

	if !strings.HasPrefix(stdout, "tickline-dev ") {
		t.Fatalf("unexpected version output: %q", stdout)
	}

	if stderr != "" {
		t.Fatalf("expected empty stderr, got %q", stderr)
	}
}

func TestUnknownCommand(t *testing.T) {
	exitCode, stdout, stderr := runCommand(t, "unknown")

	if exitCode != cli.ExitInvalidUsage {
		t.Fatalf(
			"expected exit code %d, got %d",
			cli.ExitInvalidUsage,
			exitCode,
		)
	}

	if stdout != "" {
		t.Fatalf("expected empty stdout, got %q", stdout)
	}

	if !strings.Contains(stderr, "unknown command: unknown") {
		t.Fatalf("unexpected stderr: %q", stderr)
	}
}

func TestCheckRejectsUnsupportedArguments(t *testing.T) {
	exitCode, stdout, stderr := runCommand(
		t,
		"check",
		"--invalid",
	)

	if exitCode != cli.ExitInvalidUsage {
		t.Fatalf(
			"expected exit code %d, got %d",
			cli.ExitInvalidUsage,
			exitCode,
		)
	}

	if stdout != "" {
		t.Fatalf(
			"expected empty stdout, got %q",
			stdout,
		)
	}

	if !strings.Contains(
		stderr,
		"flag provided but not defined: -invalid",
	) {
		t.Fatalf(
			"unexpected stderr: %q",
			stderr,
		)
	}
}

func TestMissingOutputDependencies(t *testing.T) {
	exitCode := cli.Run(nil, cli.Dependencies{})

	if exitCode != cli.ExitInternalError {
		t.Fatalf(
			"expected exit code %d, got %d",
			cli.ExitInternalError,
			exitCode,
		)
	}
}
