package runlog

import (
	"crypto/rand"
	"errors"
	"fmt"
	"io"
	"os"
	"path"
	"path/filepath"
	"sync"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
)

const entropySize = 6

type stageFiles struct {
	stdout   *os.File
	stderr   *os.File
	combined *os.File
}

type Store struct {
	mu                sync.Mutex
	runID             string
	relativeDirectory string
	absoluteDirectory string
	stages            map[string]*stageFiles
	closed            bool
}

func Create(repositoryRoot string) (*Store, error) {
	return create(
		repositoryRoot,
		time.Now().UTC(),
		rand.Reader,
	)
}

func create(
	repositoryRoot string,
	now time.Time,
	random io.Reader,
) (*Store, error) {
	root, err := filepath.Abs(repositoryRoot)
	if err != nil {
		return nil, fmt.Errorf(
			"resolve repository root: %w",
			err,
		)
	}

	info, err := os.Stat(root)
	if err != nil {
		return nil, fmt.Errorf(
			"inspect repository root: %w",
			err,
		)
	}

	if !info.IsDir() {
		return nil, fmt.Errorf(
			"repository root is not a directory: %s",
			root,
		)
	}

	runID, err := newRunID(now, random)
	if err != nil {
		return nil, err
	}

	relativeDirectory := path.Join(
		"reports",
		"check-local",
		runID,
	)

	parentDirectory := filepath.Join(
		root,
		"reports",
		"check-local",
	)

	if err := os.MkdirAll(parentDirectory, 0o755); err != nil {
		return nil, fmt.Errorf(
			"create log parent directory: %w",
			err,
		)
	}

	absoluteDirectory := filepath.Join(
		parentDirectory,
		runID,
	)

	if err := os.Mkdir(absoluteDirectory, 0o755); err != nil {
		return nil, fmt.Errorf(
			"create run log directory: %w",
			err,
		)
	}

	return &Store{
		runID:             runID,
		relativeDirectory: relativeDirectory,
		absoluteDirectory: absoluteDirectory,
		stages:            make(map[string]*stageFiles),
	}, nil
}

func newRunID(
	now time.Time,
	random io.Reader,
) (string, error) {
	var entropy [entropySize]byte

	if _, err := io.ReadFull(random, entropy[:]); err != nil {
		return "", fmt.Errorf(
			"generate run identifier entropy: %w",
			err,
		)
	}

	timestamp := now.UTC().Format(
		"20060102T150405.000000000Z",
	)

	return fmt.Sprintf(
		"%s-%x",
		timestamp,
		entropy,
	), nil
}

func (store *Store) RunID() string {
	return store.runID
}

func (store *Store) RelativeDirectory() string {
	return store.relativeDirectory
}

func (store *Store) StageCombinedPath(
	stageID string,
) string {
	return path.Join(
		store.relativeDirectory,
		stageID+".combined.log",
	)
}

func (store *Store) Observe(
	event runner.Event,
) error {
	store.mu.Lock()
	defer store.mu.Unlock()

	if store.closed {
		return errors.New("run log store is closed")
	}

	switch event.Kind {
	case runner.EventStageStarted:
		_, err := store.ensureStage(event.StageID)
		return err

	case runner.EventStageOutput:
		files, err := store.ensureStage(event.StageID)
		if err != nil {
			return err
		}

		var streamFile *os.File

		switch event.Stream {
		case runner.StreamStdout:
			streamFile = files.stdout

		case runner.StreamStderr:
			streamFile = files.stderr

		default:
			return fmt.Errorf(
				"stage %q produced unknown stream %q",
				event.StageID,
				event.Stream,
			)
		}

		if err := writeAll(streamFile, event.Data); err != nil {
			return fmt.Errorf(
				"write stage %q %s log: %w",
				event.StageID,
				event.Stream,
				err,
			)
		}

		if err := writeAll(files.combined, event.Data); err != nil {
			return fmt.Errorf(
				"write stage %q combined log: %w",
				event.StageID,
				err,
			)
		}
	}

	return nil
}

func (store *Store) Close() error {
	store.mu.Lock()
	defer store.mu.Unlock()

	if store.closed {
		return nil
	}

	store.closed = true

	var result error

	for stageID, files := range store.stages {
		result = errors.Join(
			result,
			closeFile(stageID, "stdout", files.stdout),
			closeFile(stageID, "stderr", files.stderr),
			closeFile(stageID, "combined", files.combined),
		)
	}

	return result
}

func (store *Store) ensureStage(
	stageID string,
) (*stageFiles, error) {
	if existing, ok := store.stages[stageID]; ok {
		return existing, nil
	}

	stdout, err := store.createFile(
		stageID + ".stdout.log",
	)
	if err != nil {
		return nil, err
	}

	stderr, err := store.createFile(
		stageID + ".stderr.log",
	)
	if err != nil {
		_ = stdout.Close()
		return nil, err
	}

	combined, err := store.createFile(
		stageID + ".combined.log",
	)
	if err != nil {
		_ = stdout.Close()
		_ = stderr.Close()
		return nil, err
	}

	files := &stageFiles{
		stdout:   stdout,
		stderr:   stderr,
		combined: combined,
	}

	store.stages[stageID] = files

	return files, nil
}

func (store *Store) createFile(
	name string,
) (*os.File, error) {
	file, err := os.OpenFile(
		filepath.Join(store.absoluteDirectory, name),
		os.O_WRONLY|os.O_CREATE|os.O_EXCL,
		0o644,
	)
	if err != nil {
		return nil, fmt.Errorf(
			"create run log %q: %w",
			name,
			err,
		)
	}

	return file, nil
}

func writeAll(
	writer io.Writer,
	data []byte,
) error {
	for len(data) != 0 {
		written, err := writer.Write(data)
		if err != nil {
			return err
		}

		if written == 0 {
			return io.ErrShortWrite
		}

		data = data[written:]
	}

	return nil
}

func closeFile(
	stageID string,
	kind string,
	file *os.File,
) error {
	if file == nil {
		return nil
	}

	if err := file.Close(); err != nil {
		return fmt.Errorf(
			"close stage %q %s log: %w",
			stageID,
			kind,
			err,
		)
	}

	return nil
}
