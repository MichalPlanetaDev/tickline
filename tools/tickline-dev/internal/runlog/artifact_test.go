package runlog_test

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runlog"
)

func TestStoreWritesExclusiveRunArtifact(
	t *testing.T,
) {
	root := t.TempDir()

	store, err := runlog.Create(root)
	if err != nil {
		t.Fatalf("create store: %v", err)
	}

	relativePath, err := store.WriteArtifact(
		"result.json",
		[]byte("{\"status\":\"passed\"}\n"),
	)
	if err != nil {
		t.Fatalf("write artifact: %v", err)
	}

	expectedRelativePath := filepath.ToSlash(
		filepath.Join(
			store.RelativeDirectory(),
			"result.json",
		),
	)

	if relativePath != expectedRelativePath {
		t.Fatalf(
			"expected path %q, got %q",
			expectedRelativePath,
			relativePath,
		)
	}

	absolutePath := filepath.Join(
		root,
		filepath.FromSlash(relativePath),
	)

	content, err := os.ReadFile(absolutePath)
	if err != nil {
		t.Fatalf("read artifact: %v", err)
	}

	if string(content) !=
		"{\"status\":\"passed\"}\n" {
		t.Fatalf(
			"unexpected artifact content: %q",
			content,
		)
	}

	if _, err := store.WriteArtifact(
		"result.json",
		[]byte("{}"),
	); err == nil {
		t.Fatal(
			"expected duplicate artifact write to fail",
		)
	}

	if err := store.Close(); err != nil {
		t.Fatalf("close store: %v", err)
	}
}

func TestStoreRejectsArtifactPathTraversal(
	t *testing.T,
) {
	root := t.TempDir()

	store, err := runlog.Create(root)
	if err != nil {
		t.Fatalf("create store: %v", err)
	}
	defer func() {
		_ = store.Close()
	}()

	for _, name := range []string{
		"",
		".",
		"..",
		"../result.json",
		"nested/result.json",
		`nested\result.json`,
	} {
		if _, err := store.WriteArtifact(
			name,
			[]byte("{}"),
		); err == nil {
			t.Fatalf(
				"expected artifact name %q to fail",
				name,
			)
		}
	}
}
