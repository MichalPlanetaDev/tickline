package artifactverify

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/artifactmanifest"
)

const SchemaVersion = 1

type Status string

const (
	StatusPassed Status = "passed"
	StatusFailed Status = "failed"
)

var ErrManifestOutsideRepository = errors.New(
	"artifact manifest is outside the repository",
)

type Report struct {
	SchemaVersion int    `json:"schema_version"`
	Status        Status `json:"status"`
	ManifestPath  string `json:"manifest_path"`
	RunID         string `json:"run_id,omitempty"`
	ArtifactCount int    `json:"artifact_count"`
	Error         string `json:"error,omitempty"`
}

func VerifyFile(
	repositoryRoot string,
	workingDirectory string,
	manifestPath string,
) (Report, error) {
	absolutePath, relativePath, err := resolveManifestPath(
		repositoryRoot,
		workingDirectory,
		manifestPath,
	)
	if err != nil {
		return Report{}, err
	}

	report := Report{
		SchemaVersion: SchemaVersion,
		Status:        StatusFailed,
		ManifestPath:  relativePath,
	}

	data, err := os.ReadFile(absolutePath)
	if err != nil {
		return Report{}, fmt.Errorf(
			"read artifact manifest %q: %w",
			relativePath,
			err,
		)
	}

	manifest, err := artifactmanifest.Parse(data)
	if err != nil {
		report.Error = err.Error()
		return report, nil
	}

	report.RunID = manifest.RunID
	report.ArtifactCount = len(manifest.Artifacts)

	if err := artifactmanifest.Verify(
		repositoryRoot,
		manifest,
	); err != nil {
		report.Error = err.Error()
		return report, nil
	}

	report.Status = StatusPassed

	return report, nil
}

func resolveManifestPath(
	repositoryRoot string,
	workingDirectory string,
	manifestPath string,
) (string, string, error) {
	if strings.TrimSpace(manifestPath) == "" {
		return "", "", errors.New(
			"artifact manifest path is empty",
		)
	}

	absoluteRoot, err := filepath.Abs(repositoryRoot)
	if err != nil {
		return "", "", fmt.Errorf(
			"resolve repository root: %w",
			err,
		)
	}

	rootInfo, err := os.Stat(absoluteRoot)
	if err != nil {
		return "", "", fmt.Errorf(
			"inspect repository root: %w",
			err,
		)
	}

	if !rootInfo.IsDir() {
		return "", "", fmt.Errorf(
			"repository root is not a directory: %s",
			absoluteRoot,
		)
	}

	baseDirectory := workingDirectory
	if strings.TrimSpace(baseDirectory) == "" {
		baseDirectory = absoluteRoot
	}

	absoluteBase, err := filepath.Abs(baseDirectory)
	if err != nil {
		return "", "", fmt.Errorf(
			"resolve working directory: %w",
			err,
		)
	}

	candidate := manifestPath

	if !filepath.IsAbs(candidate) {
		candidate = filepath.Join(
			absoluteBase,
			candidate,
		)
	}

	absoluteCandidate, err := filepath.Abs(candidate)
	if err != nil {
		return "", "", fmt.Errorf(
			"resolve artifact manifest path: %w",
			err,
		)
	}

	if outsideRepository(
		absoluteRoot,
		absoluteCandidate,
	) {
		return "", "", fmt.Errorf(
			"%w: %s",
			ErrManifestOutsideRepository,
			manifestPath,
		)
	}

	resolvedRoot, err := filepath.EvalSymlinks(
		absoluteRoot,
	)
	if err != nil {
		return "", "", fmt.Errorf(
			"resolve repository root links: %w",
			err,
		)
	}

	resolvedCandidate, err := filepath.EvalSymlinks(
		absoluteCandidate,
	)
	if err != nil {
		return "", "", fmt.Errorf(
			"resolve artifact manifest links: %w",
			err,
		)
	}

	if outsideRepository(
		resolvedRoot,
		resolvedCandidate,
	) {
		return "", "", fmt.Errorf(
			"%w: %s",
			ErrManifestOutsideRepository,
			manifestPath,
		)
	}

	information, err := os.Stat(resolvedCandidate)
	if err != nil {
		return "", "", fmt.Errorf(
			"inspect artifact manifest: %w",
			err,
		)
	}

	if !information.Mode().IsRegular() {
		return "", "", fmt.Errorf(
			"artifact manifest is not a regular file: %s",
			manifestPath,
		)
	}

	relativePath, err := filepath.Rel(
		resolvedRoot,
		resolvedCandidate,
	)
	if err != nil {
		return "", "", fmt.Errorf(
			"resolve repository-relative manifest path: %w",
			err,
		)
	}

	return resolvedCandidate,
		filepath.ToSlash(relativePath),
		nil
}

func outsideRepository(
	repositoryRoot string,
	candidate string,
) bool {
	relativePath, err := filepath.Rel(
		repositoryRoot,
		candidate,
	)
	if err != nil {
		return true
	}

	return relativePath == ".." ||
		filepath.IsAbs(relativePath) ||
		strings.HasPrefix(
			relativePath,
			".."+string(filepath.Separator),
		)
}
