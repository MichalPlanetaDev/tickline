//go:build !linux

package runner

import (
	"errors"
	"os"
	"os/exec"
)

func configureProcessTree(_ *exec.Cmd) {
}

func terminateProcessTree(process *os.Process) error {
	if process == nil {
		return nil
	}

	err := process.Signal(os.Interrupt)
	if err == nil || errors.Is(err, os.ErrProcessDone) {
		return nil
	}

	return err
}

func killProcessTree(process *os.Process) error {
	if process == nil {
		return nil
	}

	err := process.Kill()
	if err == nil || errors.Is(err, os.ErrProcessDone) {
		return nil
	}

	return err
}

func terminationSignal(_ *os.ProcessState) string {
	return ""
}
