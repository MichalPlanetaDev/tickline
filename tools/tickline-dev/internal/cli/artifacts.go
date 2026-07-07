package cli

import (
	"errors"

	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/artifactmanifest"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/report/jsonreport"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runlog"
	"github.com/MichalPlanetaDev/tickline/tools/tickline-dev/internal/runner"
)

const artifactManifestName = "artifacts.json"

func persistRunArtifacts(
	repositoryRoot string,
	logStore *runlog.Store,
	result runner.RunResult,
	logWriteError error,
) (runner.RunResult, error) {
	result.RunID = logStore.RunID()
	result.LogDirectory = logStore.RelativeDirectory()
	result.ResultPath = logStore.ArtifactPath(
		resultArtifactName,
	)
	result.ArtifactManifestPath = logStore.ArtifactPath(
		artifactManifestName,
	)

	targets := []artifactmanifest.Target{
		{
			Path: result.ResultPath,
			Kind: artifactmanifest.KindResult,
		},
	}

	for index := range result.Stages {
		stage := &result.Stages[index]

		if stage.StartedAt.IsZero() {
			continue
		}

		stage.LogPath = logStore.StageCombinedPath(
			stage.ID,
		)

		targets = append(
			targets,
			artifactmanifest.Target{
				Path: logStore.StageStdoutPath(
					stage.ID,
				),
				Kind:    artifactmanifest.KindStdoutLog,
				StageID: stage.ID,
			},
			artifactmanifest.Target{
				Path: logStore.StageStderrPath(
					stage.ID,
				),
				Kind:    artifactmanifest.KindStderrLog,
				StageID: stage.ID,
			},
			artifactmanifest.Target{
				Path:    stage.LogPath,
				Kind:    artifactmanifest.KindCombinedLog,
				StageID: stage.ID,
			},
		)
	}

	reportData, reportError := jsonreport.Marshal(result)

	var resultWriteError error

	if reportError == nil {
		_, resultWriteError = logStore.WriteArtifact(
			resultArtifactName,
			reportData,
		)
	}

	var manifestBuildError error
	var manifestMarshalError error
	var manifestWriteError error

	if logWriteError == nil &&
		reportError == nil &&
		resultWriteError == nil {
		manifest, err := artifactmanifest.Build(
			repositoryRoot,
			result.RunID,
			targets,
		)

		if err != nil {
			manifestBuildError = err
		} else {
			manifestData, err :=
				artifactmanifest.Marshal(manifest)

			if err != nil {
				manifestMarshalError = err
			} else {
				_, manifestWriteError =
					logStore.WriteArtifact(
						artifactManifestName,
						manifestData,
					)
			}
		}
	}

	closeError := logStore.Close()

	return result, errors.Join(
		logWriteError,
		reportError,
		resultWriteError,
		manifestBuildError,
		manifestMarshalError,
		manifestWriteError,
		closeError,
	)
}
