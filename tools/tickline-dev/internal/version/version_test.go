package version

import "testing"

func TestCurrent(t *testing.T) {
	const expected = "0.6.0"

	if Current != expected {
		t.Fatalf("Current = %q, expected %q", Current, expected)
	}
}
