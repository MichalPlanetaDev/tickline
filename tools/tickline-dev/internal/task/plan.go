package task

import (
	"fmt"
	"strings"
)

func (manifest Manifest) BuildPlan(
	selection Selection,
) ([]Task, error) {
	tasksByID := make(map[string]Task, len(manifest.Tasks))

	for _, current := range manifest.Tasks {
		tasksByID[current.ID] = current
	}

	skipped, err := validateSelectionIDs(
		selection.Skip,
		tasksByID,
		"skip",
	)
	if err != nil {
		return nil, err
	}

	selected := make(map[string]struct{}, len(manifest.Tasks))

	if len(selection.Only) == 0 {
		for _, current := range manifest.Tasks {
			if current.EnabledByDefault {
				selected[current.ID] = struct{}{}
			}
		}
	} else {
		only, err := validateSelectionIDs(
			selection.Only,
			tasksByID,
			"only",
		)
		if err != nil {
			return nil, err
		}

		for id := range only {
			selected[id] = struct{}{}
		}
	}

	for id := range skipped {
		if _, explicitlySelected := selected[id]; explicitlySelected && len(selection.Only) != 0 {
			return nil, fmt.Errorf(
				"stage %q is present in both only and skip selections",
				id,
			)
		}

		delete(selected, id)
	}

	ordered := make([]Task, 0, len(selected))
	state := make(map[string]uint8, len(manifest.Tasks))

	var visit func(string) error

	visit = func(id string) error {
		switch state[id] {
		case 1:
			return fmt.Errorf(
				"dependency cycle encountered at stage %q",
				id,
			)

		case 2:
			return nil
		}

		if _, isSkipped := skipped[id]; isSkipped {
			return fmt.Errorf(
				"stage %q is required but explicitly skipped",
				id,
			)
		}

		current, exists := tasksByID[id]
		if !exists {
			return fmt.Errorf(
				"execution plan references unknown stage %q",
				id,
			)
		}

		state[id] = 1

		for _, dependency := range current.Dependencies {
			if err := visit(dependency); err != nil {
				return fmt.Errorf(
					"stage %q dependency resolution failed: %w",
					id,
					err,
				)
			}
		}

		state[id] = 2
		ordered = append(ordered, cloneTask(current))

		return nil
	}

	for _, current := range manifest.Tasks {
		if _, isSelected := selected[current.ID]; !isSelected {
			continue
		}

		if err := visit(current.ID); err != nil {
			return nil, err
		}
	}

	if len(ordered) == 0 {
		return nil, fmt.Errorf(
			"task selection produced an empty execution plan",
		)
	}

	return ordered, nil
}

func validateSelectionIDs(
	values []string,
	tasksByID map[string]Task,
	kind string,
) (map[string]struct{}, error) {
	result := make(map[string]struct{}, len(values))

	for _, raw := range values {
		id := strings.TrimSpace(raw)

		if id == "" {
			return nil, fmt.Errorf(
				"%s selection contains an empty stage identifier",
				kind,
			)
		}

		if _, exists := tasksByID[id]; !exists {
			return nil, fmt.Errorf(
				"%s selection references unknown stage %q",
				kind,
				id,
			)
		}

		if _, duplicate := result[id]; duplicate {
			return nil, fmt.Errorf(
				"%s selection repeats stage %q",
				kind,
				id,
			)
		}

		result[id] = struct{}{}
	}

	return result, nil
}

func cloneTask(current Task) Task {
	cloned := current
	cloned.Dependencies = append(
		[]string(nil),
		current.Dependencies...,
	)

	return cloned
}
