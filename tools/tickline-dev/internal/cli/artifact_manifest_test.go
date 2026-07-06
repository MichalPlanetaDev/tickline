package cli_test

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/artifactmanifest"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/cli"
)

func TestCheckCreatesVerifiableArtifactManifest(
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

	resultMatches, err := filepath.Glob(
		filepath.Join(
			root,
			"reports",
			"check-local",
			"*",
			"result.json",
		),
	)
	if err != nil {
		t.Fatalf("find result document: %v", err)
	}

	if len(resultMatches) != 1 {
		t.Fatalf(
			"expected one result document, got %d",
			len(resultMatches),
		)
	}

	resultData, err := os.ReadFile(resultMatches[0])
	if err != nil {
		t.Fatalf("read result document: %v", err)
	}

	var result struct {
		SchemaVersion        int    `json:"schema_version"`
		ArtifactManifestPath string `json:"artifact_manifest_path"`
	}

	if err := json.Unmarshal(
		resultData,
		&result,
	); err != nil {
		t.Fatalf("decode result document: %v", err)
	}

	if result.SchemaVersion != 2 {
		t.Fatalf(
			"expected schema version 2, got %d",
			result.SchemaVersion,
		)
	}

	if result.ArtifactManifestPath == "" {
		t.Fatal("expected artifact manifest path")
	}

	manifestData, err := os.ReadFile(
		filepath.Join(
			root,
			filepath.FromSlash(
				result.ArtifactManifestPath,
			),
		),
	)
	if err != nil {
		t.Fatalf("read artifact manifest: %v", err)
	}

	manifest, err := artifactmanifest.Parse(
		manifestData,
	)
	if err != nil {
		t.Fatalf("parse artifact manifest: %v", err)
	}

	if len(manifest.Artifacts) != 4 {
		t.Fatalf(
			"expected 4 artifacts, got %d",
			len(manifest.Artifacts),
		)
	}

	if err := artifactmanifest.Verify(
		root,
		manifest,
	); err != nil {
		t.Fatalf("verify artifact manifest: %v", err)
	}

	if !strings.Contains(
		stdout.String(),
		"Artifacts: "+result.ArtifactManifestPath,
	) {
		t.Fatalf(
			"plain output did not expose manifest path: %q",
			stdout.String(),
		)
	}

	var stdoutLog string

	for _, artifact := range manifest.Artifacts {
		if artifact.Kind ==
			artifactmanifest.KindStdoutLog {
			stdoutLog = artifact.Path
			break
		}
	}

	if stdoutLog == "" {
		t.Fatal("stdout log is absent from manifest")
	}

	file, err := os.OpenFile(
		filepath.Join(
			root,
			filepath.FromSlash(stdoutLog),
		),
		os.O_WRONLY|os.O_APPEND,
		0,
	)
	if err != nil {
		t.Fatalf("open stdout log: %v", err)
	}

	if _, err := file.WriteString("tampered\n"); err != nil {
		_ = file.Close()
		t.Fatalf("modify stdout log: %v", err)
	}

	if err := file.Close(); err != nil {
		t.Fatalf("close stdout log: %v", err)
	}

	if err := artifactmanifest.Verify(
		root,
		manifest,
	); err == nil {
		t.Fatal(
			"expected verification to detect modified log",
		)
	}
}
