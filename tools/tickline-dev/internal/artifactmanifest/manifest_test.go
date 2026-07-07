package artifactmanifest

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestBuildMarshalParseAndVerify(
	t *testing.T,
) {
	root := t.TempDir()

	resultPath :=
		"reports/check-local/fixture/result.json"

	stdoutPath :=
		"reports/check-local/fixture/docs.stdout.log"

	writeFixture(
		t,
		root,
		resultPath,
		"{\"status\":\"passed\"}\n",
	)

	writeFixture(
		t,
		root,
		stdoutPath,
		"documentation passed\n",
	)

	manifest, err := Build(
		root,
		"fixture",
		[]Target{
			{
				Path:    stdoutPath,
				Kind:    KindStdoutLog,
				StageID: "docs",
			},
			{
				Path: resultPath,
				Kind: KindResult,
			},
		},
	)
	if err != nil {
		t.Fatalf("build manifest: %v", err)
	}

	if len(manifest.Artifacts) != 2 {
		t.Fatalf(
			"expected 2 artifacts, got %d",
			len(manifest.Artifacts),
		)
	}

	if manifest.Artifacts[0].Path != stdoutPath ||
		manifest.Artifacts[1].Path != resultPath {
		t.Fatalf(
			"artifacts are not sorted: %#v",
			manifest.Artifacts,
		)
	}

	data, err := Marshal(manifest)
	if err != nil {
		t.Fatalf("marshal manifest: %v", err)
	}

	parsed, err := Parse(data)
	if err != nil {
		t.Fatalf("parse manifest: %v", err)
	}

	if err := Verify(root, parsed); err != nil {
		t.Fatalf("verify manifest: %v", err)
	}
}

func TestVerifyDetectsModifiedArtifact(
	t *testing.T,
) {
	root := t.TempDir()

	relativePath :=
		"reports/check-local/fixture/result.json"

	writeFixture(
		t,
		root,
		relativePath,
		"original\n",
	)

	manifest, err := Build(
		root,
		"fixture",
		[]Target{
			{
				Path: relativePath,
				Kind: KindResult,
			},
		},
	)
	if err != nil {
		t.Fatalf("build manifest: %v", err)
	}

	writeFixture(
		t,
		root,
		relativePath,
		"modified\n",
	)

	err = Verify(root, manifest)

	if err == nil ||
		(!strings.Contains(err.Error(), "size mismatch") &&
			!strings.Contains(err.Error(), "SHA-256 mismatch")) {
		t.Fatalf(
			"expected integrity failure, got %v",
			err,
		)
	}
}

func TestBuildRejectsDuplicateTargetPath(
	t *testing.T,
) {
	root := t.TempDir()

	relativePath :=
		"reports/check-local/fixture/result.json"

	writeFixture(
		t,
		root,
		relativePath,
		"{}\n",
	)

	_, err := Build(
		root,
		"fixture",
		[]Target{
			{
				Path: relativePath,
				Kind: KindResult,
			},
			{
				Path: relativePath,
				Kind: KindResult,
			},
		},
	)

	if err == nil ||
		!strings.Contains(err.Error(), "duplicated") {
		t.Fatalf(
			"expected duplicate-path error, got %v",
			err,
		)
	}
}

func TestBuildRejectsEscapingPath(
	t *testing.T,
) {
	_, err := Build(
		t.TempDir(),
		"fixture",
		[]Target{
			{
				Path: "../outside.json",
				Kind: KindResult,
			},
		},
	)

	if err == nil ||
		!strings.Contains(
			err.Error(),
			"invalid artifact path",
		) {
		t.Fatalf(
			"expected invalid-path error, got %v",
			err,
		)
	}
}

func TestParseRejectsUnknownFields(
	t *testing.T,
) {
	data := []byte(`{
  "schema_version": 1,
  "run_id": "fixture",
  "artifacts": [],
  "unexpected": true
}`)

	_, err := Parse(data)

	if err == nil ||
		!strings.Contains(err.Error(), "unknown field") {
		t.Fatalf(
			"expected unknown-field error, got %v",
			err,
		)
	}
}

func TestBuildAndVerifyRejectSymbolicLinkArtifact(
	t *testing.T,
) {
	root := t.TempDir()

	targetPath :=
		"reports/check-local/fixture/target.json"

	linkPath :=
		"reports/check-local/fixture/result.json"

	writeFixture(
		t,
		root,
		targetPath,
		"{}\n",
	)

	absoluteTarget := filepath.Join(
		root,
		filepath.FromSlash(targetPath),
	)

	absoluteLink := filepath.Join(
		root,
		filepath.FromSlash(linkPath),
	)

	if err := os.Symlink(
		filepath.Base(absoluteTarget),
		absoluteLink,
	); err != nil {
		t.Skipf(
			"symbolic links are unavailable: %v",
			err,
		)
	}

	_, err := Build(
		root,
		"fixture",
		[]Target{
			{
				Path: linkPath,
				Kind: KindResult,
			},
		},
	)

	if err == nil ||
		!strings.Contains(
			err.Error(),
			"symbolic link",
		) {
		t.Fatalf(
			"expected symbolic-link error, got %v",
			err,
		)
	}

	manifest, err := Build(
		root,
		"fixture",
		[]Target{
			{
				Path: targetPath,
				Kind: KindResult,
			},
		},
	)
	if err != nil {
		t.Fatalf("build direct-file manifest: %v", err)
	}

	manifest.Artifacts[0].Path = linkPath

	err = Verify(root, manifest)

	if err == nil ||
		!strings.Contains(
			err.Error(),
			"symbolic link",
		) {
		t.Fatalf(
			"expected verification rejection, got %v",
			err,
		)
	}
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
