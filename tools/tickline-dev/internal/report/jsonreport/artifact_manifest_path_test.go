package jsonreport_test

import (
	"encoding/json"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/report/jsonreport"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
)

func TestMarshalIncludesArtifactManifestPath(
	t *testing.T,
) {
	data, err := jsonreport.Marshal(
		runner.RunResult{
			RunID: "fixture-run",
			ArtifactManifestPath: "reports/check-local/" +
				"fixture-run/artifacts.json",
			Status: runner.StatusPassed,
		},
	)
	if err != nil {
		t.Fatalf("marshal report: %v", err)
	}

	var decoded struct {
		SchemaVersion        int    `json:"schema_version"`
		ArtifactManifestPath string `json:"artifact_manifest_path"`
	}

	if err := json.Unmarshal(data, &decoded); err != nil {
		t.Fatalf("decode report: %v", err)
	}

	if decoded.SchemaVersion != 2 {
		t.Fatalf(
			"expected schema version 2, got %d",
			decoded.SchemaVersion,
		)
	}

	expected := "reports/check-local/" +
		"fixture-run/artifacts.json"

	if decoded.ArtifactManifestPath != expected {
		t.Fatalf(
			"expected manifest path %q, got %q",
			expected,
			decoded.ArtifactManifestPath,
		)
	}
}
