package main

import (
	"context"
	"os"
	"os/signal"
	"strings"
	"syscall"

	"golang.org/x/term"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/cli"
)

func main() {
	ctx, stop := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
		syscall.SIGTERM,
	)
	defer stop()

	exitCode := cli.Run(
		os.Args[1:],
		cli.Dependencies{
			Context:           ctx,
			Stdin:             os.Stdin,
			Stdout:            os.Stdout,
			Stderr:            os.Stderr,
			TerminalAvailable: terminalIsAvailable(),
		},
	)

	os.Exit(exitCode)
}

func terminalIsAvailable() bool {
	if strings.EqualFold(
		strings.TrimSpace(os.Getenv("TERM")),
		"dumb",
	) {
		return false
	}

	return term.IsTerminal(int(os.Stdin.Fd())) &&
		term.IsTerminal(int(os.Stdout.Fd()))
}
