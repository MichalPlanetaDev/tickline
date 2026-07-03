package main

import (
	"os"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/cli"
)

func main() {
	exitCode := cli.Run(
		os.Args[1:],
		cli.Dependencies{
			Stdout: os.Stdout,
			Stderr: os.Stderr,
		},
	)

	os.Exit(exitCode)
}
