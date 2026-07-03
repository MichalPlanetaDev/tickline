package task_test

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

func TestParseValidManifest(t *testing.T) {
	manifest, err := task.Parse(strings.NewReader(
		"version\t1\n" +
			"stage\tdocs\tDocumentation\tscripts/checks/docs.sh\ttrue\t\n" +
			"stage\tcpp\tC++\tscripts/checks/cpp.sh\ttrue\tdocs\n",
	))
	if err != nil {
		t.Fatalf("parse valid manifest: %v", err)
	}

	if manifest.Version != 1 {
		t.Fatalf(
			"expected version 1, got %d",
			manifest.Version,
		)
	}

	if len(manifest.Tasks) != 2 {
		t.Fatalf(
			"expected 2 tasks, got %d",
			len(manifest.Tasks),
		)
	}

	if manifest.Tasks[1].Dependencies[0] != "docs" {
		t.Fatalf(
			"unexpected dependency list: %#v",
			manifest.Tasks[1].Dependencies,
		)
	}
}

func TestParseRejectsStageBeforeVersion(t *testing.T) {
	_, err := task.Parse(strings.NewReader(
		"stage\tdocs\tDocumentation\tscripts/checks/docs.sh\ttrue\t\n",
	))

	if err == nil ||
		!strings.Contains(
			err.Error(),
			"version record must precede",
		) {
		t.Fatalf(
			"expected version-order error, got %v",
			err,
		)
	}
}

func TestValidateRejectsDuplicateIdentifier(t *testing.T) {
	root := createRepository(
		t,
		"scripts/checks/docs.sh",
	)

	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:         "docs",
				Label:      "Docs",
				ScriptPath: "scripts/checks/docs.sh",
			},
			{
				ID:         "docs",
				Label:      "Docs again",
				ScriptPath: "scripts/checks/docs.sh",
			},
		},
	}

	err := manifest.Validate(root)

	if err == nil ||
		!strings.Contains(err.Error(), "duplicated") {
		t.Fatalf(
			"expected duplicate error, got %v",
			err,
		)
	}
}

func TestValidateRejectsMissingDependency(t *testing.T) {
	root := createRepository(
		t,
		"scripts/checks/cpp.sh",
	)

	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:           "cpp",
				Label:        "C++",
				ScriptPath:   "scripts/checks/cpp.sh",
				Dependencies: []string{"docs"},
			},
		},
	}

	err := manifest.Validate(root)

	if err == nil ||
		!strings.Contains(err.Error(), "unknown stage") {
		t.Fatalf(
			"expected missing dependency error, got %v",
			err,
		)
	}
}

func TestValidateRejectsDependencyCycle(t *testing.T) {
	root := createRepository(
		t,
		"scripts/checks/a.sh",
		"scripts/checks/b.sh",
	)

	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:           "a",
				Label:        "A",
				ScriptPath:   "scripts/checks/a.sh",
				Dependencies: []string{"b"},
			},
			{
				ID:           "b",
				Label:        "B",
				ScriptPath:   "scripts/checks/b.sh",
				Dependencies: []string{"a"},
			},
		},
	}

	err := manifest.Validate(root)

	if err == nil ||
		!strings.Contains(err.Error(), "dependency cycle") {
		t.Fatalf(
			"expected cycle error, got %v",
			err,
		)
	}
}

func TestValidateRejectsEscapingScriptPath(t *testing.T) {
	root := createRepository(t)

	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:         "docs",
				Label:      "Docs",
				ScriptPath: "../outside.sh",
			},
		},
	}

	err := manifest.Validate(root)

	if err == nil ||
		!strings.Contains(
			err.Error(),
			"escapes the repository",
		) {
		t.Fatalf(
			"expected path error, got %v",
			err,
		)
	}
}

func TestBuildPlanIncludesDependenciesInStableOrder(
	t *testing.T,
) {
	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:               "docs",
				Label:            "Docs",
				EnabledByDefault: false,
				ManifestOrder:    0,
			},
			{
				ID:               "python",
				Label:            "Python",
				EnabledByDefault: true,
				ManifestOrder:    1,
			},
			{
				ID:               "cpp",
				Label:            "C++",
				EnabledByDefault: true,
				Dependencies:     []string{"docs"},
				ManifestOrder:    2,
			},
			{
				ID:               "docker",
				Label:            "Docker",
				EnabledByDefault: true,
				Dependencies:     []string{"cpp"},
				ManifestOrder:    3,
			},
		},
	}

	plan, err := manifest.BuildPlan(task.Selection{
		Only: []string{"docker"},
	})
	if err != nil {
		t.Fatalf("build plan: %v", err)
	}

	assertTaskIDs(
		t,
		plan,
		"docs",
		"cpp",
		"docker",
	)
}

func TestBuildPlanRejectsSkippedDependency(t *testing.T) {
	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:    "cpp",
				Label: "C++",
			},
			{
				ID:           "docker",
				Label:        "Docker",
				Dependencies: []string{"cpp"},
			},
		},
	}

	_, err := manifest.BuildPlan(task.Selection{
		Only: []string{"docker"},
		Skip: []string{"cpp"},
	})

	if err == nil ||
		!strings.Contains(
			err.Error(),
			"required but explicitly skipped",
		) {
		t.Fatalf(
			"expected skipped dependency error, got %v",
			err,
		)
	}
}

func TestBuildPlanRejectsUnknownSelection(t *testing.T) {
	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:    "cpp",
				Label: "C++",
			},
		},
	}

	_, err := manifest.BuildPlan(task.Selection{
		Only: []string{"missing"},
	})

	if err == nil ||
		!strings.Contains(err.Error(), "unknown stage") {
		t.Fatalf(
			"expected unknown selection error, got %v",
			err,
		)
	}
}

func createRepository(
	t *testing.T,
	scripts ...string,
) string {
	t.Helper()

	root := t.TempDir()

	for _, script := range scripts {
		path := filepath.Join(
			root,
			filepath.FromSlash(script),
		)

		if err := os.MkdirAll(
			filepath.Dir(path),
			0o755,
		); err != nil {
			t.Fatalf(
				"create script directory: %v",
				err,
			)
		}

		if err := os.WriteFile(
			path,
			[]byte("#!/usr/bin/env bash\n"),
			0o755,
		); err != nil {
			t.Fatalf(
				"create script: %v",
				err,
			)
		}
	}

	return root
}

func assertTaskIDs(
	t *testing.T,
	tasks []task.Task,
	expected ...string,
) {
	t.Helper()

	if len(tasks) != len(expected) {
		t.Fatalf(
			"expected %d tasks, got %d",
			len(expected),
			len(tasks),
		)
	}

	for index, expectedID := range expected {
		if tasks[index].ID != expectedID {
			t.Fatalf(
				"task %d: expected %q, got %q",
				index,
				expectedID,
				tasks[index].ID,
			)
		}
	}
}
