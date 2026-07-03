package cli

import (
	"strings"
	"testing"
)

func TestOutputModeUsesTUIForInteractiveTerminal(
	t *testing.T,
) {
	mode, err := resolveCheckOutputMode(
		false,
		false,
		false,
		false,
		true,
	)
	if err != nil {
		t.Fatalf("resolve output mode: %v", err)
	}

	if mode != checkOutputTUI {
		t.Fatalf(
			"expected TUI mode, got %d",
			mode,
		)
	}
}

func TestOutputModeFallsBackToPlainWithoutTerminal(
	t *testing.T,
) {
	mode, err := resolveCheckOutputMode(
		false,
		false,
		false,
		false,
		false,
	)
	if err != nil {
		t.Fatalf("resolve output mode: %v", err)
	}

	if mode != checkOutputPlain {
		t.Fatalf(
			"expected plain mode, got %d",
			mode,
		)
	}
}

func TestOutputModeRejectsForcedTUIWithoutTerminal(
	t *testing.T,
) {
	_, err := resolveCheckOutputMode(
		false,
		false,
		false,
		true,
		false,
	)

	if err == nil {
		t.Fatal("expected forced TUI to fail")
	}

	if !strings.Contains(
		err.Error(),
		"interactive",
	) {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestOutputModeRejectsConflictingModes(
	t *testing.T,
) {
	_, err := resolveCheckOutputMode(
		false,
		true,
		true,
		false,
		true,
	)

	if err == nil {
		t.Fatal("expected conflicting modes to fail")
	}

	if !strings.Contains(
		err.Error(),
		"mutually exclusive",
	) {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestOutputModeRejectsPlanWithPresentationMode(
	t *testing.T,
) {
	_, err := resolveCheckOutputMode(
		true,
		true,
		false,
		false,
		true,
	)

	if err == nil {
		t.Fatal("expected plan conflict")
	}

	if !strings.Contains(
		err.Error(),
		"cannot be combined",
	) {
		t.Fatalf("unexpected error: %v", err)
	}
}
