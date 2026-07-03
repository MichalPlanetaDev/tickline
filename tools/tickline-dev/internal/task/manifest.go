package task

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

var taskIDPattern = regexp.MustCompile(`^[a-z0-9]+(?:-[a-z0-9]+)*$`)

func Load(manifestPath, repositoryRoot string) (Manifest, error) {
	file, err := os.Open(manifestPath)
	if err != nil {
		return Manifest{}, fmt.Errorf("open task manifest: %w", err)
	}
	defer file.Close()

	manifest, err := Parse(file)
	if err != nil {
		return Manifest{}, err
	}

	if err := manifest.Validate(repositoryRoot); err != nil {
		return Manifest{}, err
	}

	return manifest, nil
}

func Parse(reader io.Reader) (Manifest, error) {
	scanner := bufio.NewScanner(reader)
	scanner.Buffer(make([]byte, 64*1024), 1024*1024)

	manifest := Manifest{}
	seenVersion := false
	lineNumber := 0

	for scanner.Scan() {
		lineNumber++
		line := strings.TrimSuffix(scanner.Text(), "\r")
		trimmed := strings.TrimSpace(line)

		if trimmed == "" || strings.HasPrefix(trimmed, "#") {
			continue
		}

		fields := strings.Split(line, "\t")
		recordType := fields[0]

		switch recordType {
		case "version":
			if seenVersion || len(manifest.Tasks) != 0 {
				return Manifest{}, fmt.Errorf(
					"manifest line %d: version record must appear exactly once before stages",
					lineNumber,
				)
			}

			if len(fields) != 2 {
				return Manifest{}, fmt.Errorf(
					"manifest line %d: version record requires 2 fields",
					lineNumber,
				)
			}

			version, err := strconv.Atoi(fields[1])
			if err != nil || version != 1 {
				return Manifest{}, fmt.Errorf(
					"manifest line %d: unsupported schema version %q",
					lineNumber,
					fields[1],
				)
			}

			manifest.Version = version
			seenVersion = true

		case "stage":
			if !seenVersion {
				return Manifest{}, fmt.Errorf(
					"manifest line %d: version record must precede stages",
					lineNumber,
				)
			}

			if len(fields) != 6 {
				return Manifest{}, fmt.Errorf(
					"manifest line %d: stage record requires 6 fields",
					lineNumber,
				)
			}

			enabled, err := parseEnabled(fields[4])
			if err != nil {
				return Manifest{}, fmt.Errorf(
					"manifest line %d: %w",
					lineNumber,
					err,
				)
			}

			manifest.Tasks = append(manifest.Tasks, Task{
				ID:               fields[1],
				Label:            fields[2],
				ScriptPath:       fields[3],
				EnabledByDefault: enabled,
				Dependencies:     parseDependencies(fields[5]),
				ManifestOrder:    len(manifest.Tasks),
			})

		default:
			return Manifest{}, fmt.Errorf(
				"manifest line %d: unknown record type %q",
				lineNumber,
				recordType,
			)
		}
	}

	if err := scanner.Err(); err != nil {
		return Manifest{}, fmt.Errorf("read task manifest: %w", err)
	}

	if !seenVersion {
		return Manifest{}, fmt.Errorf(
			"task manifest does not contain a version record",
		)
	}

	if len(manifest.Tasks) == 0 {
		return Manifest{}, fmt.Errorf(
			"task manifest does not contain any stages",
		)
	}

	return manifest, nil
}

func (manifest Manifest) Validate(repositoryRoot string) error {
	if manifest.Version != 1 {
		return fmt.Errorf(
			"unsupported task manifest version %d",
			manifest.Version,
		)
	}

	absoluteRoot, err := filepath.Abs(repositoryRoot)
	if err != nil {
		return fmt.Errorf("resolve repository root: %w", err)
	}

	tasksByID := make(map[string]Task, len(manifest.Tasks))

	for _, current := range manifest.Tasks {
		if !taskIDPattern.MatchString(current.ID) {
			return fmt.Errorf(
				"stage %q has an invalid identifier",
				current.ID,
			)
		}

		if strings.TrimSpace(current.Label) == "" {
			return fmt.Errorf(
				"stage %q has an empty label",
				current.ID,
			)
		}

		if _, exists := tasksByID[current.ID]; exists {
			return fmt.Errorf(
				"stage identifier %q is duplicated",
				current.ID,
			)
		}

		if err := validateScriptPath(absoluteRoot, current); err != nil {
			return err
		}

		tasksByID[current.ID] = current
	}

	for _, current := range manifest.Tasks {
		seenDependencies := make(
			map[string]struct{},
			len(current.Dependencies),
		)

		for _, dependency := range current.Dependencies {
			if dependency == current.ID {
				return fmt.Errorf(
					"stage %q depends on itself",
					current.ID,
				)
			}

			if _, exists := tasksByID[dependency]; !exists {
				return fmt.Errorf(
					"stage %q depends on unknown stage %q",
					current.ID,
					dependency,
				)
			}

			if _, duplicate := seenDependencies[dependency]; duplicate {
				return fmt.Errorf(
					"stage %q repeats dependency %q",
					current.ID,
					dependency,
				)
			}

			seenDependencies[dependency] = struct{}{}
		}
	}

	if err := validateAcyclic(manifest.Tasks, tasksByID); err != nil {
		return err
	}

	return nil
}

func parseEnabled(value string) (bool, error) {
	switch value {
	case "true":
		return true, nil
	case "false":
		return false, nil
	default:
		return false, fmt.Errorf(
			"invalid enabled_by_default value %q",
			value,
		)
	}
}

func parseDependencies(value string) []string {
	if value == "" {
		return nil
	}

	parts := strings.Split(value, ",")
	dependencies := make([]string, 0, len(parts))

	for _, part := range parts {
		dependencies = append(
			dependencies,
			strings.TrimSpace(part),
		)
	}

	return dependencies
}

func validateScriptPath(repositoryRoot string, current Task) error {
	if current.ScriptPath == "" {
		return fmt.Errorf(
			"stage %q has an empty script path",
			current.ID,
		)
	}

	if filepath.IsAbs(current.ScriptPath) {
		return fmt.Errorf(
			"stage %q uses an absolute script path",
			current.ID,
		)
	}

	cleanPath := filepath.Clean(current.ScriptPath)
	if cleanPath == "." || cleanPath != current.ScriptPath {
		return fmt.Errorf(
			"stage %q script path is not canonical: %q",
			current.ID,
			current.ScriptPath,
		)
	}

	absoluteScript := filepath.Join(repositoryRoot, cleanPath)
	relativePath, err := filepath.Rel(repositoryRoot, absoluteScript)

	if err != nil ||
		relativePath == ".." ||
		strings.HasPrefix(
			relativePath,
			".."+string(filepath.Separator),
		) {
		return fmt.Errorf(
			"stage %q script path escapes the repository",
			current.ID,
		)
	}

	info, err := os.Stat(absoluteScript)
	if err != nil {
		return fmt.Errorf(
			"stage %q script is unavailable: %w",
			current.ID,
			err,
		)
	}

	if !info.Mode().IsRegular() {
		return fmt.Errorf(
			"stage %q script is not a regular file",
			current.ID,
		)
	}

	if info.Mode().Perm()&0o111 == 0 {
		return fmt.Errorf(
			"stage %q script is not executable",
			current.ID,
		)
	}

	resolvedRoot, err := filepath.EvalSymlinks(repositoryRoot)
	if err != nil {
		return fmt.Errorf(
			"resolve repository root symlinks: %w",
			err,
		)
	}

	resolvedScript, err := filepath.EvalSymlinks(absoluteScript)
	if err != nil {
		return fmt.Errorf(
			"resolve stage %q script symlinks: %w",
			current.ID,
			err,
		)
	}

	resolvedRelative, err := filepath.Rel(
		resolvedRoot,
		resolvedScript,
	)

	if err != nil ||
		resolvedRelative == ".." ||
		strings.HasPrefix(
			resolvedRelative,
			".."+string(filepath.Separator),
		) {
		return fmt.Errorf(
			"stage %q script resolves outside the repository",
			current.ID,
		)
	}

	return nil
}

func validateAcyclic(
	tasks []Task,
	tasksByID map[string]Task,
) error {
	const (
		unvisited = iota
		visiting
		visited
	)

	state := make(map[string]int, len(tasks))
	stack := make([]string, 0, len(tasks))

	var visit func(string) error

	visit = func(id string) error {
		switch state[id] {
		case visiting:
			cycleStart := 0

			for cycleStart < len(stack) &&
				stack[cycleStart] != id {
				cycleStart++
			}

			cycle := append(
				append([]string{}, stack[cycleStart:]...),
				id,
			)

			return fmt.Errorf(
				"task dependency cycle: %s",
				strings.Join(cycle, " -> "),
			)

		case visited:
			return nil
		}

		state[id] = visiting
		stack = append(stack, id)

		for _, dependency := range tasksByID[id].Dependencies {
			if err := visit(dependency); err != nil {
				return err
			}
		}

		stack = stack[:len(stack)-1]
		state[id] = visited

		return nil
	}

	for _, current := range tasks {
		if err := visit(current.ID); err != nil {
			return err
		}
	}

	return nil
}
