package workflow

import (
	"fmt"
	"regexp"
	"strings"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

var identifierPattern = regexp.MustCompile(
	`^[a-z0-9]+(?:-[a-z0-9]+)*$`,
)

type Catalog struct {
	definitions []Definition
}

func NewCatalog(
	definitions []Definition,
) (Catalog, error) {
	seen := make(map[string]struct{}, len(definitions))
	copied := make([]Definition, 0, len(definitions))

	for _, definition := range definitions {
		if err := validateDefinition(definition); err != nil {
			return Catalog{}, err
		}

		if _, exists := seen[definition.ID]; exists {
			return Catalog{}, fmt.Errorf(
				"workflow identifier %q is duplicated",
				definition.ID,
			)
		}

		seen[definition.ID] = struct{}{}
		copied = append(
			copied,
			cloneDefinition(definition),
		)
	}

	if len(copied) == 0 {
		return Catalog{}, fmt.Errorf(
			"workflow catalog must not be empty",
		)
	}

	return Catalog{
		definitions: copied,
	}, nil
}

func BuiltInCatalog() Catalog {
	catalog, err := NewCatalog(
		[]Definition{
			{
				ID:    "release-readiness",
				Label: "Release readiness",
				Description: "Run the repository verification stages " +
					"required before a merge or release.",
				Selection: task.Selection{},
				StagePurposes: []StagePurpose{
					{
						TaskID:  "docs",
						Purpose: "Verify documentation structure, required files, and public claims.",
					},
					{
						TaskID:  "cpp",
						Purpose: "Build and test the native deterministic simulation, protocol, evidence, and storage layers.",
					},
					{
						TaskID:  "sanitizers",
						Purpose: "Execute native tests with memory and undefined-behavior instrumentation.",
					},
					{
						TaskID:  "python",
						Purpose: "Verify investigation analytics, reporting, review, and command-line behavior.",
					},
					{
						TaskID:  "go",
						Purpose: "Verify the developer console, process supervision, reporting, and terminal-independent logic.",
					},
					{
						TaskID:  "docker",
						Purpose: "Build the reproducible container verification environment.",
					},
				},
				Artifacts: []Artifact{
					{
						ID:          "result-json",
						Label:       "Canonical result",
						PathPattern: "reports/check-local/<run-id>/result.json",
						Description: "Versioned machine-readable run result.",
						Required:    true,
					},
					{
						ID:          "combined-stage-log",
						Label:       "Combined stage log",
						PathPattern: "reports/check-local/<run-id>/<stage-id>.combined.log",
						Description: "Ordered standard-output and standard-error stream for one executed stage.",
						Required:    true,
					},
					{
						ID:          "stream-stage-logs",
						Label:       "Stage stream logs",
						PathPattern: "reports/check-local/<run-id>/<stage-id>.<stdout|stderr>.log",
						Description: "Separate standard-output and standard-error logs for one executed stage.",
						Required:    true,
					},
				},
			},
		},
	)
	if err != nil {
		panic(err)
	}

	return catalog
}

func (catalog Catalog) List() []Definition {
	result := make(
		[]Definition,
		0,
		len(catalog.definitions),
	)

	for _, definition := range catalog.definitions {
		result = append(
			result,
			cloneDefinition(definition),
		)
	}

	return result
}

func (catalog Catalog) Resolve(
	id string,
	manifest task.Manifest,
) (Plan, error) {
	var definition *Definition

	for index := range catalog.definitions {
		if catalog.definitions[index].ID == id {
			definition = &catalog.definitions[index]
			break
		}
	}

	if definition == nil {
		return Plan{}, fmt.Errorf(
			"unknown workflow %q",
			id,
		)
	}

	executionPlan, err := manifest.BuildPlan(
		definition.Selection,
	)
	if err != nil {
		return Plan{}, fmt.Errorf(
			"resolve workflow %q stages: %w",
			id,
			err,
		)
	}

	purposes := make(
		map[string]string,
		len(definition.StagePurposes),
	)

	for _, current := range definition.StagePurposes {
		purposes[current.TaskID] = current.Purpose
	}

	stages := make(
		[]PlannedStage,
		0,
		len(executionPlan),
	)

	for _, current := range executionPlan {
		purpose, exists := purposes[current.ID]
		if !exists {
			return Plan{}, fmt.Errorf(
				"workflow %q has no purpose for stage %q",
				id,
				current.ID,
			)
		}

		stages = append(
			stages,
			PlannedStage{
				ID:         current.ID,
				Label:      current.Label,
				Purpose:    purpose,
				ScriptPath: current.ScriptPath,
				Dependencies: append(
					[]string{},
					current.Dependencies...,
				),
			},
		)
	}

	return Plan{
		Definition: cloneDefinition(*definition),
		Stages:     stages,
	}, nil
}

func validateDefinition(
	definition Definition,
) error {
	if !identifierPattern.MatchString(definition.ID) {
		return fmt.Errorf(
			"workflow %q has an invalid identifier",
			definition.ID,
		)
	}

	if strings.TrimSpace(definition.Label) == "" {
		return fmt.Errorf(
			"workflow %q has an empty label",
			definition.ID,
		)
	}

	if strings.TrimSpace(definition.Description) == "" {
		return fmt.Errorf(
			"workflow %q has an empty description",
			definition.ID,
		)
	}

	stageIDs := make(
		map[string]struct{},
		len(definition.StagePurposes),
	)

	for _, current := range definition.StagePurposes {
		if !identifierPattern.MatchString(current.TaskID) {
			return fmt.Errorf(
				"workflow %q has invalid stage identifier %q",
				definition.ID,
				current.TaskID,
			)
		}

		if strings.TrimSpace(current.Purpose) == "" {
			return fmt.Errorf(
				"workflow %q stage %q has an empty purpose",
				definition.ID,
				current.TaskID,
			)
		}

		if _, exists := stageIDs[current.TaskID]; exists {
			return fmt.Errorf(
				"workflow %q repeats stage purpose %q",
				definition.ID,
				current.TaskID,
			)
		}

		stageIDs[current.TaskID] = struct{}{}
	}

	if len(stageIDs) == 0 {
		return fmt.Errorf(
			"workflow %q has no stage purposes",
			definition.ID,
		)
	}

	artifactIDs := make(
		map[string]struct{},
		len(definition.Artifacts),
	)

	for _, artifact := range definition.Artifacts {
		if !identifierPattern.MatchString(artifact.ID) {
			return fmt.Errorf(
				"workflow %q has invalid artifact identifier %q",
				definition.ID,
				artifact.ID,
			)
		}

		if strings.TrimSpace(artifact.Label) == "" ||
			strings.TrimSpace(artifact.PathPattern) == "" ||
			strings.TrimSpace(artifact.Description) == "" {
			return fmt.Errorf(
				"workflow %q artifact %q has incomplete metadata",
				definition.ID,
				artifact.ID,
			)
		}

		if _, exists := artifactIDs[artifact.ID]; exists {
			return fmt.Errorf(
				"workflow %q repeats artifact %q",
				definition.ID,
				artifact.ID,
			)
		}

		artifactIDs[artifact.ID] = struct{}{}
	}

	return nil
}

func cloneDefinition(
	definition Definition,
) Definition {
	cloned := definition

	cloned.Selection.Only = append(
		[]string(nil),
		definition.Selection.Only...,
	)
	cloned.Selection.Skip = append(
		[]string(nil),
		definition.Selection.Skip...,
	)
	cloned.StagePurposes = append(
		[]StagePurpose(nil),
		definition.StagePurposes...,
	)
	cloned.Artifacts = append(
		[]Artifact(nil),
		definition.Artifacts...,
	)

	return cloned
}
