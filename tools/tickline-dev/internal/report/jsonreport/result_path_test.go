package jsonreport_test

import (
	"encoding/json"
	"testing"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/report/jsonreport"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
)

func TestMarshalIncludesCanonicalResultPath(
	t *testing.T,
) {
	data, err := jsonreport.Marshal(
		runner.RunResult{
			RunID: "fixture-run",
			LogDirectory: "reports/check-local/" +
				"fixture-run",
			ResultPath: "reports/check-local/" +
				"fixture-run/result.json",
			Status: runner.StatusPassed,
		},
	)
	if err != nil {
		t.Fatalf("marshal report: %v", err)
	}

	var decoded struct {
		ResultPath string `json:"result_path"`
	}

	if err := json.Unmarshal(
		data,
		&decoded,
	); err != nil {
		t.Fatalf("decode report: %v", err)
	}

	expected := "reports/check-local/" +
		"fixture-run/result.json"

	if decoded.ResultPath != expected {
		t.Fatalf(
			"expected result path %q, got %q",
			expected,
			decoded.ResultPath,
		)
	}
}
