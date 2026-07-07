package workflow

import (
	"strings"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

func TestReleaseReadinessResolvesManifestPlan(
	t *testing.T,
) {
	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:               "docs",
				Label:            "Documentation",
				ScriptPath:       "scripts/checks/docs.sh",
				EnabledByDefault: true,
				ManifestOrder:    0,
			},
			{
				ID:               "cpp",
				Label:            "C++",
				ScriptPath:       "scripts/checks/cpp.sh",
				EnabledByDefault: true,
				Dependencies:     []string{"docs"},
				ManifestOrder:    1,
			},
			{
				ID:               "python",
				Label:            "Python",
				ScriptPath:       "scripts/checks/python.sh",
				EnabledByDefault: true,
				ManifestOrder:    2,
			},
		},
	}

	plan, err := BuiltInCatalog().Resolve(
		"release-readiness",
		manifest,
	)
	if err != nil {
		t.Fatalf("resolve workflow: %v", err)
	}

	if plan.Definition.ID != "release-readiness" {
		t.Fatalf(
			"unexpected workflow identifier: %q",
			plan.Definition.ID,
		)
	}

	expectedIDs := []string{
		"docs",
		"cpp",
		"python",
	}

	if len(plan.Stages) != len(expectedIDs) {
		t.Fatalf(
			"expected %d stages, got %d",
			len(expectedIDs),
			len(plan.Stages),
		)
	}

	for index, expectedID := range expectedIDs {
		stage := plan.Stages[index]

		if stage.ID != expectedID {
			t.Fatalf(
				"stage %d: expected %q, got %q",
				index,
				expectedID,
				stage.ID,
			)
		}

		if strings.TrimSpace(stage.Purpose) == "" {
			t.Fatalf(
				"stage %q has no purpose",
				stage.ID,
			)
		}
	}

	if len(plan.Definition.Artifacts) != 4 {
		t.Fatalf(
			"expected 4 artifact definitions, got %d",
			len(plan.Definition.Artifacts),
		)
	}
}

func TestResolveRejectsUnknownWorkflow(
	t *testing.T,
) {
	_, err := BuiltInCatalog().Resolve(
		"missing",
		task.Manifest{},
	)

	if err == nil ||
		!strings.Contains(err.Error(), "unknown workflow") {
		t.Fatalf(
			"expected unknown-workflow error, got %v",
			err,
		)
	}
}

func TestResolveRejectsStageWithoutPurpose(
	t *testing.T,
) {
	manifest := task.Manifest{
		Version: 1,
		Tasks: []task.Task{
			{
				ID:               "unexpected",
				Label:            "Unexpected",
				ScriptPath:       "scripts/checks/unexpected.sh",
				EnabledByDefault: true,
			},
		},
	}

	_, err := BuiltInCatalog().Resolve(
		"release-readiness",
		manifest,
	)

	if err == nil ||
		!strings.Contains(
			err.Error(),
			`has no purpose for stage "unexpected"`,
		) {
		t.Fatalf(
			"expected missing-purpose error, got %v",
			err,
		)
	}
}

func TestCatalogReturnsIndependentDefinitions(
	t *testing.T,
) {
	catalog := BuiltInCatalog()
	first := catalog.List()

	first[0].Label = "modified"
	first[0].Artifacts[0].Label = "modified"

	second := catalog.List()

	if second[0].Label == "modified" {
		t.Fatal("catalog workflow was modified through list result")
	}

	if second[0].Artifacts[0].Label == "modified" {
		t.Fatal("catalog artifact was modified through list result")
	}
}

func TestCatalogRejectsDuplicateWorkflowIdentifiers(
	t *testing.T,
) {
	definition := BuiltInCatalog().List()[0]

	_, err := NewCatalog(
		[]Definition{
			definition,
			definition,
		},
	)

	if err == nil ||
		!strings.Contains(err.Error(), "duplicated") {
		t.Fatalf(
			"expected duplicate identifier error, got %v",
			err,
		)
	}
}
