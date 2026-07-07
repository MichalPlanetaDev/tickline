package cli_test

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/artifactmanifest"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/artifactverify"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/cli"
)

func TestArtifactsVerifyPlainPasses(
	t *testing.T,
) {
	root := createCheckRepository(t)
	manifestPath := createCLIArtifactManifest(
		t,
		root,
		"fixture-run",
	)

	exitCode, stdout, stderr :=
		runArtifactsCommand(
			t,
			root,
			"artifacts",
			"verify",
			manifestPath,
		)

	if exitCode != cli.ExitSuccess {
		t.Fatalf(
			"expected success, got %d: %s",
			exitCode,
			stderr,
		)
	}

	for _, expected := range []string{
		"Tickline artifact verification",
		"Manifest: " + manifestPath,
		"Run: fixture-run",
		"Artifacts: 1",
		"Result: passed",
	} {
		if !strings.Contains(stdout, expected) {
			t.Fatalf(
				"expected output to contain %q:\n%s",
				expected,
				stdout,
			)
		}
	}
}

func TestArtifactsVerifyJSONPasses(
	t *testing.T,
) {
	root := createCheckRepository(t)
	manifestPath := createCLIArtifactManifest(
		t,
		root,
		"fixture-run",
	)

	exitCode, stdout, stderr :=
		runArtifactsCommand(
			t,
			root,
			"artifacts",
			"verify",
			"--json",
			manifestPath,
		)

	if exitCode != cli.ExitSuccess {
		t.Fatalf(
			"expected success, got %d: %s",
			exitCode,
			stderr,
		)
	}

	var report artifactverify.Report

	if err := json.Unmarshal(
		[]byte(stdout),
		&report,
	); err != nil {
		t.Fatalf(
			"decode verification JSON: %v\n%s",
			err,
			stdout,
		)
	}

	if report.SchemaVersion != 1 {
		t.Fatalf(
			"expected schema version 1, got %d",
			report.SchemaVersion,
		)
	}

	if report.Status != artifactverify.StatusPassed {
		t.Fatalf(
			"expected passed status, got %q",
			report.Status,
		)
	}

	if report.ManifestPath != manifestPath {
		t.Fatalf(
			"expected manifest path %q, got %q",
			manifestPath,
			report.ManifestPath,
		)
	}
}

func TestArtifactsVerifyReturnsCheckFailureForTampering(
	t *testing.T,
) {
	root := createCheckRepository(t)
	manifestPath := createCLIArtifactManifest(
		t,
		root,
		"fixture-run",
	)

	resultPath := filepath.Join(
		root,
		"reports",
		"check-local",
		"fixture-run",
		"result.json",
	)

	if err := os.WriteFile(
		resultPath,
		[]byte("tampered\n"),
		0o644,
	); err != nil {
		t.Fatalf("tamper with result: %v", err)
	}

	exitCode, stdout, stderr :=
		runArtifactsCommand(
			t,
			root,
			"artifacts",
			"verify",
			manifestPath,
		)

	if exitCode != cli.ExitCheckFailed {
		t.Fatalf(
			"expected check failure, got %d: %s",
			exitCode,
			stderr,
		)
	}

	if !strings.Contains(stdout, "Result: failed") {
		t.Fatalf(
			"expected failed result:\n%s",
			stdout,
		)
	}

	if !strings.Contains(stdout, "mismatch") {
		t.Fatalf(
			"expected integrity mismatch:\n%s",
			stdout,
		)
	}
}

func TestArtifactsVerifyRequiresManifestPath(
	t *testing.T,
) {
	root := createCheckRepository(t)

	exitCode, _, stderr :=
		runArtifactsCommand(
			t,
			root,
			"artifacts",
			"verify",
		)

	if exitCode != cli.ExitInvalidUsage {
		t.Fatalf(
			"expected invalid usage, got %d",
			exitCode,
		)
	}

	if !strings.Contains(
		stderr,
		"artifact manifest path is required",
	) {
		t.Fatalf(
			"unexpected error output: %q",
			stderr,
		)
	}
}

func TestArtifactsVerifyRejectsMissingManifest(
	t *testing.T,
) {
	root := createCheckRepository(t)

	exitCode, _, stderr :=
		runArtifactsCommand(
			t,
			root,
			"artifacts",
			"verify",
			"reports/check-local/missing/artifacts.json",
		)

	if exitCode != cli.ExitInvalidUsage {
		t.Fatalf(
			"expected invalid usage, got %d",
			exitCode,
		)
	}

	if stderr == "" {
		t.Fatal("expected error output")
	}
}

func createCLIArtifactManifest(
	t *testing.T,
	root string,
	runID string,
) string {
	t.Helper()

	resultPath :=
		"reports/check-local/" +
			runID +
			"/result.json"

	manifestPath :=
		"reports/check-local/" +
			runID +
			"/artifacts.json"

	absoluteResultPath := filepath.Join(
		root,
		filepath.FromSlash(resultPath),
	)

	if err := os.MkdirAll(
		filepath.Dir(absoluteResultPath),
		0o755,
	); err != nil {
		t.Fatalf(
			"create artifact directory: %v",
			err,
		)
	}

	if err := os.WriteFile(
		absoluteResultPath,
		[]byte("{\"status\":\"passed\"}\n"),
		0o644,
	); err != nil {
		t.Fatalf("write result: %v", err)
	}

	manifest, err := artifactmanifest.Build(
		root,
		runID,
		[]artifactmanifest.Target{
			{
				Path: resultPath,
				Kind: artifactmanifest.KindResult,
			},
		},
	)
	if err != nil {
		t.Fatalf("build manifest: %v", err)
	}

	data, err := artifactmanifest.Marshal(manifest)
	if err != nil {
		t.Fatalf("marshal manifest: %v", err)
	}

	absoluteManifestPath := filepath.Join(
		root,
		filepath.FromSlash(manifestPath),
	)

	if err := os.WriteFile(
		absoluteManifestPath,
		data,
		0o644,
	); err != nil {
		t.Fatalf("write manifest: %v", err)
	}

	return manifestPath
}

func runArtifactsCommand(
	t *testing.T,
	root string,
	args ...string,
) (int, string, string) {
	t.Helper()

	var stdout bytes.Buffer
	var stderr bytes.Buffer

	exitCode := cli.Run(
		args,
		cli.Dependencies{
			Stdout:           &stdout,
			Stderr:           &stderr,
			WorkingDirectory: root,
		},
	)

	return exitCode,
		stdout.String(),
		stderr.String()
}
