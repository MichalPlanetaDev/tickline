package main

import (
	"context"
	"os"
	"os/signal"
	"syscall"

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
			Context: ctx,
			Stdout:  os.Stdout,
			Stderr:  os.Stderr,
		},
	)

	os.Exit(exitCode)
}
