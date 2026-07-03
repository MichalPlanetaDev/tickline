//go:build linux

package runner

import (
	"errors"
	"os"
	"os/exec"
	"syscall"
)

func configureProcessTree(command *exec.Cmd) {
	command.SysProcAttr = &syscall.SysProcAttr{
		Setpgid: true,
	}
}

func terminateProcessTree(process *os.Process) error {
	return signalProcessGroup(process, syscall.SIGTERM)
}

func killProcessTree(process *os.Process) error {
	return signalProcessGroup(process, syscall.SIGKILL)
}

func signalProcessGroup(
	process *os.Process,
	signal syscall.Signal,
) error {
	if process == nil {
		return nil
	}

	err := syscall.Kill(-process.Pid, signal)
	if err == nil || errors.Is(err, syscall.ESRCH) {
		return nil
	}

	return err
}

func terminationSignal(state *os.ProcessState) string {
	if state == nil {
		return ""
	}

	waitStatus, ok := state.Sys().(syscall.WaitStatus)
	if !ok || !waitStatus.Signaled() {
		return ""
	}

	return waitStatus.Signal().String()
}
