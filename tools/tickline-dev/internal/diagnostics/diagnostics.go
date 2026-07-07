package diagnostics

import (
	"context"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"time"
)

const SchemaVersion = 1

const probeTimeout = 5 * time.Second

type Status string

const (
	StatusPassed Status = "passed"
	StatusFailed Status = "failed"
)

type ToolStatus string

const (
	ToolAvailable ToolStatus = "available"
	ToolMissing   ToolStatus = "missing"
	ToolError     ToolStatus = "error"
)

type Platform struct {
	OS   string `json:"os"`
	Arch string `json:"arch"`
	WSL  bool   `json:"wsl"`
}

type ToolResult struct {
	ID       string     `json:"id"`
	Label    string     `json:"label"`
	Command  string     `json:"command"`
	Required bool       `json:"required"`
	Status   ToolStatus `json:"status"`
	Path     string     `json:"path,omitempty"`
	Version  string     `json:"version,omitempty"`
	Error    string     `json:"error,omitempty"`
}

type Report struct {
	SchemaVersion  int          `json:"schema_version"`
	Status         Status       `json:"status"`
	RepositoryRoot string       `json:"repository_root"`
	Platform       Platform     `json:"platform"`
	Tools          []ToolResult `json:"tools"`
}

type toolSpec struct {
	id      string
	label   string
	command string
	args    []string
}

var requiredTools = []toolSpec{
	{
		id:      "bash",
		label:   "Bash",
		command: "bash",
		args:    []string{"--version"},
	},
	{
		id:      "cmake",
		label:   "CMake",
		command: "cmake",
		args:    []string{"--version"},
	},
	{
		id:      "cpp",
		label:   "C++ compiler",
		command: "c++",
		args:    []string{"--version"},
	},
	{
		id:      "python",
		label:   "Python",
		command: "python3",
		args:    []string{"--version"},
	},
	{
		id:      "go",
		label:   "Go",
		command: "go",
		args:    []string{"version"},
	},
	{
		id:      "docker",
		label:   "Docker",
		command: "docker",
		args:    []string{"--version"},
	},
	{
		id:      "git",
		label:   "Git",
		command: "git",
		args:    []string{"--version"},
	},
}

func Probe(
	ctx context.Context,
	repositoryRoot string,
) (Report, error) {
	if ctx == nil {
		ctx = context.Background()
	}

	report := Report{
		SchemaVersion:  SchemaVersion,
		Status:         StatusPassed,
		RepositoryRoot: repositoryRoot,
		Platform: Platform{
			OS:   runtime.GOOS,
			Arch: runtime.GOARCH,
			WSL:  detectWSL(),
		},
		Tools: make([]ToolResult, 0, len(requiredTools)),
	}

	for _, spec := range requiredTools {
		if err := ctx.Err(); err != nil {
			return Report{}, err
		}

		result, err := probeTool(ctx, spec)
		if err != nil {
			return Report{}, err
		}

		if result.Status != ToolAvailable {
			report.Status = StatusFailed
		}

		report.Tools = append(report.Tools, result)
	}

	return report, nil
}

func probeTool(
	ctx context.Context,
	spec toolSpec,
) (ToolResult, error) {
	result := ToolResult{
		ID:       spec.id,
		Label:    spec.label,
		Command:  spec.command,
		Required: true,
		Status:   ToolMissing,
	}

	path, err := exec.LookPath(spec.command)
	if err != nil {
		result.Error = "executable not found in PATH"
		return result, nil
	}

	result.Path = path

	probeContext, cancel := context.WithTimeout(
		ctx,
		probeTimeout,
	)
	defer cancel()

	command := exec.CommandContext(
		probeContext,
		path,
		spec.args...,
	)

	output, commandError := command.CombinedOutput()

	if err := ctx.Err(); err != nil {
		return ToolResult{}, err
	}

	if errors.Is(
		probeContext.Err(),
		context.DeadlineExceeded,
	) {
		result.Status = ToolError
		result.Error = "version command timed out"

		return result, nil
	}

	version := normalizeVersion(output)

	if commandError != nil {
		result.Status = ToolError
		result.Error = commandError.Error()

		if version != "" {
			result.Error = fmt.Sprintf(
				"%s: %s",
				result.Error,
				version,
			)
		}

		return result, nil
	}

	if version == "" {
		result.Status = ToolError
		result.Error = "version command returned empty output"

		return result, nil
	}

	result.Status = ToolAvailable
	result.Version = version

	return result, nil
}

func normalizeVersion(output []byte) string {
	text := strings.ReplaceAll(
		string(output),
		"\r\n",
		"\n",
	)

	text = strings.TrimSpace(text)
	if text == "" {
		return ""
	}

	firstLine := strings.SplitN(
		text,
		"\n",
		2,
	)[0]

	return strings.Join(
		strings.Fields(firstLine),
		" ",
	)
}

func detectWSL() bool {
	if runtime.GOOS != "linux" {
		return false
	}

	for _, filename := range []string{
		"/proc/sys/kernel/osrelease",
		"/proc/version",
	} {
		data, err := os.ReadFile(filename)
		if err != nil {
			continue
		}

		value := strings.ToLower(string(data))

		if strings.Contains(value, "microsoft") ||
			strings.Contains(value, "wsl") {
			return true
		}
	}

	return false
}
