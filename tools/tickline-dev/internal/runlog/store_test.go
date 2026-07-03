package runlog

import (
	"bytes"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
)

func TestStoreCreatesStableRunIdentifierAndLogs(
	t *testing.T,
) {
	root := t.TempDir()

	now := time.Date(
		2026,
		time.July,
		3,
		17,
		45,
		12,
		123456789,
		time.FixedZone("fixture", 2*60*60),
	)

	store, err := create(
		root,
		now,
		bytes.NewReader([]byte{
			0xaa,
			0xbb,
			0xcc,
			0xdd,
			0xee,
			0xff,
		}),
	)
	if err != nil {
		t.Fatalf("create log store: %v", err)
	}

	expectedRunID :=
		"20260703T154512.123456789Z-aabbccddeeff"

	if store.RunID() != expectedRunID {
		t.Fatalf(
			"expected run ID %q, got %q",
			expectedRunID,
			store.RunID(),
		)
	}

	events := []runner.Event{
		{
			Kind:    runner.EventStageStarted,
			StageID: "cpp",
		},
		{
			Kind:    runner.EventStageOutput,
			StageID: "cpp",
			Stream:  runner.StreamStdout,
			Data:    []byte("configure\n"),
		},
		{
			Kind:    runner.EventStageOutput,
			StageID: "cpp",
			Stream:  runner.StreamStderr,
			Data:    []byte("warning\n"),
		},
		{
			Kind:    runner.EventStageOutput,
			StageID: "cpp",
			Stream:  runner.StreamStdout,
			Data:    []byte("complete\n"),
		},
	}

	for _, event := range events {
		if err := store.Observe(event); err != nil {
			t.Fatalf("observe event: %v", err)
		}
	}

	if err := store.Close(); err != nil {
		t.Fatalf("close log store: %v", err)
	}

	if err := store.Close(); err != nil {
		t.Fatalf(
			"second close should be harmless: %v",
			err,
		)
	}

	runDirectory := filepath.Join(
		root,
		"reports",
		"check-local",
		expectedRunID,
	)

	assertFileContent(
		t,
		filepath.Join(runDirectory, "cpp.stdout.log"),
		"configure\ncomplete\n",
	)

	assertFileContent(
		t,
		filepath.Join(runDirectory, "cpp.stderr.log"),
		"warning\n",
	)

	assertFileContent(
		t,
		filepath.Join(runDirectory, "cpp.combined.log"),
		"configure\nwarning\ncomplete\n",
	)
}

func TestNewRunIDRejectsEntropyFailure(t *testing.T) {
	_, err := newRunID(
		time.Time{},
		bytes.NewReader(nil),
	)

	if err == nil {
		t.Fatal("expected entropy failure")
	}
}

func assertFileContent(
	t *testing.T,
	path string,
	expected string,
) {
	t.Helper()

	content, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}

	if string(content) != expected {
		t.Fatalf(
			"expected %q in %s, got %q",
			expected,
			path,
			content,
		)
	}
}
