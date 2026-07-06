package workflow

import (
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/task"
)

type StagePurpose struct {
	TaskID  string
	Purpose string
}

type Artifact struct {
	ID          string
	Label       string
	PathPattern string
	Description string
	Required    bool
}

type Definition struct {
	ID            string
	Label         string
	Description   string
	Selection     task.Selection
	StagePurposes []StagePurpose
	Artifacts     []Artifact
}

type PlannedStage struct {
	ID           string
	Label        string
	Purpose      string
	ScriptPath   string
	Dependencies []string
}

type Plan struct {
	Definition Definition
	Stages     []PlannedStage
}
