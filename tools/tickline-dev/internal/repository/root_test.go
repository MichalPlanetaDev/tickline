package repository_test

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/repository"
)

func TestFindRepositoryFromNestedDirectory(t *testing.T) {
	root := createRepository(t)
	nested := filepath.Join(root, "tools", "tickline-dev", "internal")

	if err := os.MkdirAll(nested, 0o755); err != nil {
		t.Fatalf("create nested directory: %v", err)
	}

	found, err := repository.Find(nested)
	if err != nil {
		t.Fatalf("find repository: %v", err)
	}

	if found != root {
		t.Fatalf("expected root %q, got %q", root, found)
	}
}

func TestFindRejectsUnrelatedDirectory(t *testing.T) {
	_, err := repository.Find(t.TempDir())

	if err == nil {
		t.Fatal("expected repository lookup to fail")
	}

	if !strings.Contains(err.Error(), "was not found") {
		t.Fatalf("unexpected error: %v", err)
	}
}

func createRepository(t *testing.T) string {
	t.Helper()

	root := t.TempDir()

	files := []string{
		"CMakeLists.txt",
		"scripts/checks/manifest.tsv",
		"tools/tickline-dev/go.mod",
	}

	for _, relativePath := range files {
		path := filepath.Join(root, filepath.FromSlash(relativePath))

		if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
			t.Fatalf("create parent directory: %v", err)
		}

		if err := os.WriteFile(path, []byte("fixture\n"), 0o644); err != nil {
			t.Fatalf("create repository marker: %v", err)
		}
	}

	return root
}
