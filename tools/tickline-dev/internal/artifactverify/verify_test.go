package artifactverify_test

import (
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/artifactmanifest"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/artifactverify"
)

func TestVerifyFilePassesForValidManifest(
	t *testing.T,
) {
	root := t.TempDir()

	manifestPath := createFixtureManifest(
		t,
		root,
		"fixture-run",
		"original\n",
	)

	report, err := artifactverify.VerifyFile(
		root,
		root,
		manifestPath,
	)
	if err != nil {
		t.Fatalf("verify manifest: %v", err)
	}

	if report.Status != artifactverify.StatusPassed {
		t.Fatalf(
			"expected passed status, got %q: %s",
			report.Status,
			report.Error,
		)
	}

	if report.RunID != "fixture-run" {
		t.Fatalf(
			"expected run identifier %q, got %q",
			"fixture-run",
			report.RunID,
		)
	}

	if report.ArtifactCount != 1 {
		t.Fatalf(
			"expected 1 artifact, got %d",
			report.ArtifactCount,
		)
	}

	expectedPath :=
		"reports/check-local/fixture-run/artifacts.json"

	if report.ManifestPath != expectedPath {
		t.Fatalf(
			"expected manifest path %q, got %q",
			expectedPath,
			report.ManifestPath,
		)
	}
}

func TestVerifyFileReportsModifiedArtifact(
	t *testing.T,
) {
	root := t.TempDir()

	manifestPath := createFixtureManifest(
		t,
		root,
		"fixture-run",
		"original\n",
	)

	writeFixture(
		t,
		root,
		"reports/check-local/fixture-run/result.json",
		"modified\n",
	)

	report, err := artifactverify.VerifyFile(
		root,
		root,
		manifestPath,
	)
	if err != nil {
		t.Fatalf("verify manifest: %v", err)
	}

	if report.Status != artifactverify.StatusFailed {
		t.Fatalf(
			"expected failed status, got %q",
			report.Status,
		)
	}

	if !strings.Contains(
		report.Error,
		"mismatch",
	) {
		t.Fatalf(
			"expected integrity mismatch, got %q",
			report.Error,
		)
	}
}

func TestVerifyFileReportsMalformedManifest(
	t *testing.T,
) {
	root := t.TempDir()

	manifestPath :=
		"reports/check-local/fixture-run/artifacts.json"

	writeFixture(
		t,
		root,
		manifestPath,
		"{invalid-json}\n",
	)

	report, err := artifactverify.VerifyFile(
		root,
		root,
		manifestPath,
	)
	if err != nil {
		t.Fatalf("verify malformed manifest: %v", err)
	}

	if report.Status != artifactverify.StatusFailed {
		t.Fatalf(
			"expected failed status, got %q",
			report.Status,
		)
	}

	if report.Error == "" {
		t.Fatal("expected manifest parsing error")
	}
}

func TestVerifyFileRejectsPathOutsideRepository(
	t *testing.T,
) {
	root := t.TempDir()
	outsideRoot := t.TempDir()

	outsidePath := filepath.Join(
		outsideRoot,
		"artifacts.json",
	)

	if err := os.WriteFile(
		outsidePath,
		[]byte("{}\n"),
		0o644,
	); err != nil {
		t.Fatalf("write outside manifest: %v", err)
	}

	_, err := artifactverify.VerifyFile(
		root,
		root,
		outsidePath,
	)

	if !errors.Is(
		err,
		artifactverify.ErrManifestOutsideRepository,
	) {
		t.Fatalf(
			"expected outside-repository error, got %v",
			err,
		)
	}
}

func TestVerifyFileRejectsSymlinkOutsideRepository(
	t *testing.T,
) {
	root := t.TempDir()
	outsideRoot := t.TempDir()

	outsidePath := filepath.Join(
		outsideRoot,
		"artifacts.json",
	)

	if err := os.WriteFile(
		outsidePath,
		[]byte("{}\n"),
		0o644,
	); err != nil {
		t.Fatalf("write outside manifest: %v", err)
	}

	linkPath := filepath.Join(
		root,
		"artifacts.json",
	)

	if err := os.Symlink(
		outsidePath,
		linkPath,
	); err != nil {
		t.Skipf(
			"symbolic links are unavailable: %v",
			err,
		)
	}

	_, err := artifactverify.VerifyFile(
		root,
		root,
		linkPath,
	)

	if !errors.Is(
		err,
		artifactverify.ErrManifestOutsideRepository,
	) {
		t.Fatalf(
			"expected outside-repository error, got %v",
			err,
		)
	}
}

func createFixtureManifest(
	t *testing.T,
	root string,
	runID string,
	resultContent string,
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

	writeFixture(
		t,
		root,
		resultPath,
		resultContent,
	)

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

	writeFixture(
		t,
		root,
		manifestPath,
		string(data),
	)

	return manifestPath
}

func writeFixture(
	t *testing.T,
	root string,
	relativePath string,
	content string,
) {
	t.Helper()

	absolutePath := filepath.Join(
		root,
		filepath.FromSlash(relativePath),
	)

	if err := os.MkdirAll(
		filepath.Dir(absolutePath),
		0o755,
	); err != nil {
		t.Fatalf(
			"create fixture directory: %v",
			err,
		)
	}

	if err := os.WriteFile(
		absolutePath,
		[]byte(content),
		0o644,
	); err != nil {
		t.Fatalf(
			"write fixture %q: %v",
			relativePath,
			err,
		)
	}
}
