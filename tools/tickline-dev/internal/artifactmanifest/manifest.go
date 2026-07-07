package artifactmanifest

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path"
	"path/filepath"
	"sort"
	"strings"
)

const SchemaVersion = 1

type Kind string

const (
	KindResult      Kind = "result"
	KindStdoutLog   Kind = "stdout-log"
	KindStderrLog   Kind = "stderr-log"
	KindCombinedLog Kind = "combined-log"
)

type Target struct {
	Path    string
	Kind    Kind
	StageID string
}

type Artifact struct {
	Path      string `json:"path"`
	Kind      Kind   `json:"kind"`
	StageID   string `json:"stage_id,omitempty"`
	SizeBytes int64  `json:"size_bytes"`
	SHA256    string `json:"sha256"`
}

type Manifest struct {
	SchemaVersion int        `json:"schema_version"`
	RunID         string     `json:"run_id"`
	Artifacts     []Artifact `json:"artifacts"`
}

func Build(
	repositoryRoot string,
	runID string,
	targets []Target,
) (Manifest, error) {
	if strings.TrimSpace(runID) == "" {
		return Manifest{}, errors.New(
			"artifact manifest run identifier is empty",
		)
	}

	if len(targets) == 0 {
		return Manifest{}, errors.New(
			"artifact manifest has no targets",
		)
	}

	ordered := append([]Target(nil), targets...)

	sort.Slice(
		ordered,
		func(left int, right int) bool {
			return ordered[left].Path < ordered[right].Path
		},
	)

	artifacts := make(
		[]Artifact,
		0,
		len(ordered),
	)

	previousPath := ""

	for index, target := range ordered {
		if err := validateTarget(target); err != nil {
			return Manifest{}, err
		}

		if index != 0 && target.Path == previousPath {
			return Manifest{}, fmt.Errorf(
				"artifact target path %q is duplicated",
				target.Path,
			)
		}

		previousPath = target.Path

		size, digest, err := hashFile(
			repositoryRoot,
			target.Path,
		)
		if err != nil {
			return Manifest{}, fmt.Errorf(
				"hash artifact %q: %w",
				target.Path,
				err,
			)
		}

		artifacts = append(
			artifacts,
			Artifact{
				Path:      target.Path,
				Kind:      target.Kind,
				StageID:   target.StageID,
				SizeBytes: size,
				SHA256:    digest,
			},
		)
	}

	return Manifest{
		SchemaVersion: SchemaVersion,
		RunID:         runID,
		Artifacts:     artifacts,
	}, nil
}

func Marshal(manifest Manifest) ([]byte, error) {
	if err := validateManifest(manifest); err != nil {
		return nil, err
	}

	data, err := json.MarshalIndent(
		manifest,
		"",
		"  ",
	)
	if err != nil {
		return nil, fmt.Errorf(
			"encode artifact manifest: %w",
			err,
		)
	}

	return append(data, '\n'), nil
}

func Parse(data []byte) (Manifest, error) {
	decoder := json.NewDecoder(
		bytes.NewReader(data),
	)
	decoder.DisallowUnknownFields()

	var manifest Manifest

	if err := decoder.Decode(&manifest); err != nil {
		return Manifest{}, fmt.Errorf(
			"decode artifact manifest: %w",
			err,
		)
	}

	var trailing any

	if err := decoder.Decode(&trailing); !errors.Is(err, io.EOF) {
		if err == nil {
			return Manifest{}, errors.New(
				"artifact manifest contains multiple JSON values",
			)
		}

		return Manifest{}, fmt.Errorf(
			"decode artifact manifest trailer: %w",
			err,
		)
	}

	if err := validateManifest(manifest); err != nil {
		return Manifest{}, err
	}

	return manifest, nil
}

func Verify(
	repositoryRoot string,
	manifest Manifest,
) error {
	if err := validateManifest(manifest); err != nil {
		return err
	}

	for _, artifact := range manifest.Artifacts {
		size, digest, err := hashFile(
			repositoryRoot,
			artifact.Path,
		)
		if err != nil {
			return fmt.Errorf(
				"verify artifact %q: %w",
				artifact.Path,
				err,
			)
		}

		if size != artifact.SizeBytes {
			return fmt.Errorf(
				"artifact %q size mismatch: expected %d, got %d",
				artifact.Path,
				artifact.SizeBytes,
				size,
			)
		}

		if digest != artifact.SHA256 {
			return fmt.Errorf(
				"artifact %q SHA-256 mismatch",
				artifact.Path,
			)
		}
	}

	return nil
}

func validateManifest(manifest Manifest) error {
	if manifest.SchemaVersion != SchemaVersion {
		return fmt.Errorf(
			"unsupported artifact manifest schema version %d",
			manifest.SchemaVersion,
		)
	}

	if strings.TrimSpace(manifest.RunID) == "" {
		return errors.New(
			"artifact manifest run identifier is empty",
		)
	}

	if len(manifest.Artifacts) == 0 {
		return errors.New(
			"artifact manifest contains no artifacts",
		)
	}

	previousPath := ""

	for index, artifact := range manifest.Artifacts {
		target := Target{
			Path:    artifact.Path,
			Kind:    artifact.Kind,
			StageID: artifact.StageID,
		}

		if err := validateTarget(target); err != nil {
			return err
		}

		if index != 0 && artifact.Path <= previousPath {
			return errors.New(
				"artifact manifest paths are not strictly ordered",
			)
		}

		if artifact.SizeBytes < 0 {
			return fmt.Errorf(
				"artifact %q has a negative size",
				artifact.Path,
			)
		}

		if !isValidDigest(artifact.SHA256) {
			return fmt.Errorf(
				"artifact %q has an invalid SHA-256 digest",
				artifact.Path,
			)
		}

		previousPath = artifact.Path
	}

	return nil
}

func validateTarget(target Target) error {
	if err := validateRelativePath(target.Path); err != nil {
		return err
	}

	switch target.Kind {
	case KindResult:
		if target.StageID != "" {
			return errors.New(
				"result artifact must not have a stage identifier",
			)
		}

	case KindStdoutLog,
		KindStderrLog,
		KindCombinedLog:
		if strings.TrimSpace(target.StageID) == "" {
			return fmt.Errorf(
				"artifact %q requires a stage identifier",
				target.Path,
			)
		}

	default:
		return fmt.Errorf(
			"artifact %q has unsupported kind %q",
			target.Path,
			target.Kind,
		)
	}

	return nil
}

func validateRelativePath(value string) error {
	if value == "" ||
		value == "." ||
		value == ".." ||
		path.IsAbs(value) ||
		path.Clean(value) != value ||
		strings.HasPrefix(value, "../") ||
		strings.Contains(value, `\`) {
		return fmt.Errorf(
			"invalid artifact path %q",
			value,
		)
	}

	return nil
}

func isValidDigest(value string) bool {
	if len(value) != sha256.Size*2 ||
		value != strings.ToLower(value) {
		return false
	}

	decoded, err := hex.DecodeString(value)

	return err == nil &&
		len(decoded) == sha256.Size
}

func hashFile(
	repositoryRoot string,
	relativePath string,
) (int64, string, error) {
	absolutePath, err := resolveExistingPath(
		repositoryRoot,
		relativePath,
	)
	if err != nil {
		return 0, "", err
	}

	file, err := os.Open(absolutePath)
	if err != nil {
		return 0, "", err
	}
	defer file.Close()

	information, err := file.Stat()
	if err != nil {
		return 0, "", err
	}

	if !information.Mode().IsRegular() {
		return 0, "", errors.New(
			"artifact is not a regular file",
		)
	}

	hasher := sha256.New()

	size, err := io.Copy(hasher, file)
	if err != nil {
		return 0, "", err
	}

	return size,
		hex.EncodeToString(hasher.Sum(nil)),
		nil
}

func resolveExistingPath(
	repositoryRoot string,
	relativePath string,
) (string, error) {
	if err := validateRelativePath(relativePath); err != nil {
		return "", err
	}

	absoluteRoot, err := filepath.Abs(repositoryRoot)
	if err != nil {
		return "", fmt.Errorf(
			"resolve repository root: %w",
			err,
		)
	}

	resolvedRoot, err := filepath.EvalSymlinks(
		absoluteRoot,
	)
	if err != nil {
		return "", fmt.Errorf(
			"resolve repository root links: %w",
			err,
		)
	}

	candidate := filepath.Join(
		absoluteRoot,
		filepath.FromSlash(relativePath),
	)

	information, err := os.Lstat(candidate)
	if err != nil {
		return "", err
	}

	if information.Mode()&os.ModeSymlink != 0 {
		return "", errors.New(
			"artifact path is a symbolic link",
		)
	}

	resolvedCandidate, err := filepath.EvalSymlinks(
		candidate,
	)
	if err != nil {
		return "", err
	}

	relative, err := filepath.Rel(
		resolvedRoot,
		resolvedCandidate,
	)
	if err != nil {
		return "", fmt.Errorf(
			"compare artifact path with repository: %w",
			err,
		)
	}

	if relative == ".." ||
		strings.HasPrefix(
			relative,
			".."+string(filepath.Separator),
		) {
		return "", fmt.Errorf(
			"artifact path escapes repository: %q",
			relativePath,
		)
	}

	return resolvedCandidate, nil
}
