package runlog

import "path"

func (store *Store) StageStdoutPath(
	stageID string,
) string {
	return path.Join(
		store.relativeDirectory,
		stageID+".stdout.log",
	)
}

func (store *Store) StageStderrPath(
	stageID string,
) string {
	return path.Join(
		store.relativeDirectory,
		stageID+".stderr.log",
	)
}
