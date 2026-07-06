package runlog_test

import (
	"path"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runlog"
)

func TestStoreExposesStageStreamPaths(
	t *testing.T,
) {
	store, err := runlog.Create(t.TempDir())
	if err != nil {
		t.Fatalf("create store: %v", err)
	}
	defer func() {
		if err := store.Close(); err != nil {
			t.Fatalf("close store: %v", err)
		}
	}()

	expectedStdout := path.Join(
		store.RelativeDirectory(),
		"docs.stdout.log",
	)

	if actual := store.StageStdoutPath("docs"); actual != expectedStdout {
		t.Fatalf(
			"expected stdout path %q, got %q",
			expectedStdout,
			actual,
		)
	}

	expectedStderr := path.Join(
		store.RelativeDirectory(),
		"docs.stderr.log",
	)

	if actual := store.StageStderrPath("docs"); actual != expectedStderr {
		t.Fatalf(
			"expected stderr path %q, got %q",
			expectedStderr,
			actual,
		)
	}
}
