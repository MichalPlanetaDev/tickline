package repository

import (
	"fmt"
	"os"
	"path/filepath"
)

func Find(start string) (string, error) {
	if start == "" {
		current, err := os.Getwd()
		if err != nil {
			return "", fmt.Errorf("get working directory: %w", err)
		}

		start = current
	}

	current, err := filepath.Abs(start)
	if err != nil {
		return "", fmt.Errorf("resolve working directory: %w", err)
	}

	info, err := os.Stat(current)
	if err != nil {
		return "", fmt.Errorf("inspect working directory: %w", err)
	}

	if !info.IsDir() {
		current = filepath.Dir(current)
	}

	for {
		if isRepositoryRoot(current) {
			return current, nil
		}

		parent := filepath.Dir(current)
		if parent == current {
			break
		}

		current = parent
	}

	return "", fmt.Errorf(
		"Tickline repository root was not found from %q",
		start,
	)
}

func isRepositoryRoot(path string) bool {
	required := []string{
		"scripts/checks/manifest.tsv",
		"tools/tickline-dev/go.mod",
		"CMakeLists.txt",
	}

	for _, relativePath := range required {
		info, err := os.Stat(filepath.Join(path, relativePath))
		if err != nil || !info.Mode().IsRegular() {
			return false
		}
	}

	return true
}
