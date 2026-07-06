package cli

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"path/filepath"
	"strings"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/repository"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/workflow"
)

const workflowDocumentSchemaVersion = 1

type workflowListDocument struct {
	SchemaVersion int                       `json:"schema_version"`
	Workflows     []workflowSummaryDocument `json:"workflows"`
}

type workflowSummaryDocument struct {
	ID          string `json:"id"`
	Label       string `json:"label"`
	Description string `json:"description"`
}

type workflowPlanDocument struct {
	SchemaVersion int              `json:"schema_version"`
	Workflow      workflowDocument `json:"workflow"`
}

type workflowDocument struct {
	ID          string                     `json:"id"`
	Label       string                     `json:"label"`
	Description string                     `json:"description"`
	Stages      []workflowStageDocument    `json:"stages"`
	Artifacts   []workflowArtifactDocument `json:"artifacts"`
}

type workflowStageDocument struct {
	ID           string   `json:"id"`
	Label        string   `json:"label"`
	Purpose      string   `json:"purpose"`
	ScriptPath   string   `json:"script_path"`
	Dependencies []string `json:"dependencies"`
}

type workflowArtifactDocument struct {
	ID          string `json:"id"`
	Label       string `json:"label"`
	PathPattern string `json:"path_pattern"`
	Description string `json:"description"`
	Required    bool   `json:"required"`
}

func runWorkflow(
	args []string,
	dependencies Dependencies,
) int {
	if len(args) == 0 {
		printWorkflowHelp(dependencies.Stdout)
		return ExitSuccess
	}

	switch args[0] {
	case "help", "-h", "--help":
		printWorkflowHelp(dependencies.Stdout)
		return ExitSuccess

	case "list":
		return runWorkflowList(
			args[1:],
			dependencies,
		)

	case "show":
		return runWorkflowShow(
			args[1:],
			dependencies,
		)

	default:
		fmt.Fprintf(
			dependencies.Stderr,
			"unknown workflow command: %s\n\n",
			args[0],
		)
		printWorkflowHelp(dependencies.Stderr)

		return ExitInvalidUsage
	}
}

func runWorkflowList(
	args []string,
	dependencies Dependencies,
) int {
	jsonOutput, help, err := parseListArguments(args)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"workflow list: %v\n",
			err,
		)
		return ExitInvalidUsage
	}

	if help {
		printWorkflowListHelp(dependencies.Stdout)
		return ExitSuccess
	}

	definitions := workflow.BuiltInCatalog().List()

	if jsonOutput {
		document := workflowListDocument{
			SchemaVersion: workflowDocumentSchemaVersion,
			Workflows: make(
				[]workflowSummaryDocument,
				0,
				len(definitions),
			),
		}

		for _, definition := range definitions {
			document.Workflows = append(
				document.Workflows,
				workflowSummaryDocument{
					ID:          definition.ID,
					Label:       definition.Label,
					Description: definition.Description,
				},
			)
		}

		if err := writeIndentedJSON(
			dependencies.Stdout,
			document,
		); err != nil {
			fmt.Fprintf(
				dependencies.Stderr,
				"write workflow list: %v\n",
				err,
			)
			return ExitInternalError
		}

		return ExitSuccess
	}

	fmt.Fprintln(
		dependencies.Stdout,
		"Tickline workflows",
	)
	fmt.Fprintln(dependencies.Stdout)

	for _, definition := range definitions {
		fmt.Fprintf(
			dependencies.Stdout,
			"%-20s %s\n",
			definition.ID,
			definition.Label,
		)
		fmt.Fprintf(
			dependencies.Stdout,
			"  %s\n",
			definition.Description,
		)
	}

	fmt.Fprintf(
		dependencies.Stdout,
		"\nWorkflows: %d\n",
		len(definitions),
	)

	return ExitSuccess
}

func runWorkflowShow(
	args []string,
	dependencies Dependencies,
) int {
	identifier, jsonOutput, help, err :=
		parseShowArguments(args)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"workflow show: %v\n",
			err,
		)
		return ExitInvalidUsage
	}

	if help {
		printWorkflowShowHelp(dependencies.Stdout)
		return ExitSuccess
	}

	plan, err := resolveWorkflowPlan(
		identifier,
		dependencies.WorkingDirectory,
	)
	if err != nil {
		fmt.Fprintf(
			dependencies.Stderr,
			"resolve workflow: %v\n",
			err,
		)
		return ExitInvalidUsage
	}

	if jsonOutput {
		if err := writeIndentedJSON(
			dependencies.Stdout,
			workflowPlanDocument{
				SchemaVersion: workflowDocumentSchemaVersion,
				Workflow:      workflowPlanToDocument(plan),
			},
		); err != nil {
			fmt.Fprintf(
				dependencies.Stderr,
				"write workflow plan: %v\n",
				err,
			)
			return ExitInternalError
		}

		return ExitSuccess
	}

	printWorkflowPlan(
		dependencies.Stdout,
		plan,
	)

	return ExitSuccess
}

func resolveWorkflowPlan(
	identifier string,
	workingDirectory string,
) (workflow.Plan, error) {
	repositoryRoot, err := repository.Find(
		workingDirectory,
	)
	if err != nil {
		return workflow.Plan{}, fmt.Errorf(
			"resolve repository: %w",
			err,
		)
	}

	manifest, err := task.Load(
		filepath.Join(
			repositoryRoot,
			"scripts",
			"checks",
			"manifest.tsv",
		),
		repositoryRoot,
	)
	if err != nil {
		return workflow.Plan{}, fmt.Errorf(
			"load verification tasks: %w",
			err,
		)
	}

	return workflow.BuiltInCatalog().Resolve(
		identifier,
		manifest,
	)
}

func workflowPlanToDocument(
	plan workflow.Plan,
) workflowDocument {
	stages := make(
		[]workflowStageDocument,
		0,
		len(plan.Stages),
	)

	for _, current := range plan.Stages {
		stages = append(
			stages,
			workflowStageDocument{
				ID:         current.ID,
				Label:      current.Label,
				Purpose:    current.Purpose,
				ScriptPath: current.ScriptPath,
				Dependencies: append(
					[]string{},
					current.Dependencies...,
				),
			},
		)
	}

	artifacts := make(
		[]workflowArtifactDocument,
		0,
		len(plan.Definition.Artifacts),
	)

	for _, current := range plan.Definition.Artifacts {
		artifacts = append(
			artifacts,
			workflowArtifactDocument{
				ID:          current.ID,
				Label:       current.Label,
				PathPattern: current.PathPattern,
				Description: current.Description,
				Required:    current.Required,
			},
		)
	}

	return workflowDocument{
		ID:          plan.Definition.ID,
		Label:       plan.Definition.Label,
		Description: plan.Definition.Description,
		Stages:      stages,
		Artifacts:   artifacts,
	}
}

func printWorkflowPlan(
	output io.Writer,
	plan workflow.Plan,
) {
	fmt.Fprintf(
		output,
		"Workflow: %s\n",
		plan.Definition.ID,
	)
	fmt.Fprintf(
		output,
		"Label: %s\n",
		plan.Definition.Label,
	)
	fmt.Fprintf(
		output,
		"Purpose: %s\n",
		plan.Definition.Description,
	)

	fmt.Fprintln(output)
	fmt.Fprintln(output, "Stages:")

	for index, current := range plan.Stages {
		fmt.Fprintf(
			output,
			"%d. %s — %s\n",
			index+1,
			current.ID,
			current.Label,
		)
		fmt.Fprintf(
			output,
			"   Purpose: %s\n",
			current.Purpose,
		)
		fmt.Fprintf(
			output,
			"   Script: %s\n",
			current.ScriptPath,
		)

		if len(current.Dependencies) != 0 {
			fmt.Fprintf(
				output,
				"   Dependencies: %s\n",
				strings.Join(
					current.Dependencies,
					", ",
				),
			)
		}
	}

	fmt.Fprintln(output)
	fmt.Fprintln(output, "Artifacts:")

	for _, current := range plan.Definition.Artifacts {
		requirement := "optional"
		if current.Required {
			requirement = "required"
		}

		fmt.Fprintf(
			output,
			"- %s (%s)\n",
			current.Label,
			requirement,
		)
		fmt.Fprintf(
			output,
			"  Path: %s\n",
			current.PathPattern,
		)
		fmt.Fprintf(
			output,
			"  %s\n",
			current.Description,
		)
	}

	fmt.Fprintf(
		output,
		"\nStages: %d\n",
		len(plan.Stages),
	)
}

func parseListArguments(
	args []string,
) (bool, bool, error) {
	jsonOutput := false

	for _, argument := range args {
		switch argument {
		case "--json":
			if jsonOutput {
				return false, false, errors.New(
					"--json was provided more than once",
				)
			}
			jsonOutput = true

		case "-h", "--help":
			return false, true, nil

		default:
			return false, false, fmt.Errorf(
				"unexpected argument %q",
				argument,
			)
		}
	}

	return jsonOutput, false, nil
}

func parseShowArguments(
	args []string,
) (string, bool, bool, error) {
	jsonOutput := false
	identifier := ""

	for _, argument := range args {
		switch argument {
		case "--json":
			if jsonOutput {
				return "", false, false, errors.New(
					"--json was provided more than once",
				)
			}
			jsonOutput = true

		case "-h", "--help":
			return "", false, true, nil

		default:
			if strings.HasPrefix(argument, "-") {
				return "", false, false, fmt.Errorf(
					"unknown option %q",
					argument,
				)
			}

			if identifier != "" {
				return "", false, false, fmt.Errorf(
					"unexpected argument %q",
					argument,
				)
			}

			identifier = argument
		}
	}

	if identifier == "" {
		return "", false, false, errors.New(
			"workflow identifier is required",
		)
	}

	return identifier, jsonOutput, false, nil
}

func writeIndentedJSON(
	output io.Writer,
	value any,
) error {
	data, err := json.MarshalIndent(
		value,
		"",
		"  ",
	)
	if err != nil {
		return fmt.Errorf(
			"encode JSON: %w",
			err,
		)
	}

	data = append(data, '\n')

	if _, err := output.Write(data); err != nil {
		return fmt.Errorf(
			"write JSON: %w",
			err,
		)
	}

	return nil
}

func printWorkflowHelp(
	output io.Writer,
) {
	fmt.Fprintln(output, `Tickline workflows

Usage:
  tickline-dev workflow <command>

Commands:
  list       List available workflows
  show       Show a resolved workflow plan
  help       Show this help`)
}

func printWorkflowListHelp(
	output io.Writer,
) {
	fmt.Fprintln(output, `Usage:
  tickline-dev workflow list [--json]`)
}

func printWorkflowShowHelp(
	output io.Writer,
) {
	fmt.Fprintln(output, `Usage:
  tickline-dev workflow show [--json] <workflow-id>`)
}
