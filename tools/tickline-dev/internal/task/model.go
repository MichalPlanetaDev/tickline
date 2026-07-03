package task

type Task struct {
	ID               string
	Label            string
	ScriptPath       string
	EnabledByDefault bool
	Dependencies     []string
	ManifestOrder    int
}

type Manifest struct {
	Version int
	Tasks   []Task
}

type Selection struct {
	Only []string
	Skip []string
}
