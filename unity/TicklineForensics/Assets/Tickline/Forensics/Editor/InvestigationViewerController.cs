using System;

namespace Tickline.Forensics.Editor
{
    public sealed class InvestigationViewerController :
        IDisposable
    {
        private readonly InvestigationViewerSession session;

        private InvestigationWorkspace observedWorkspace;
        private bool disposed;

        public InvestigationViewerController()
        {
            session = new InvestigationViewerSession();

            session.LoadAttempted += HandleLoadAttempted;
            session.StateChanged += HandleStateChanged;
        }

        public event Action Changed;

        public InvestigationViewerSession Session => session;

        public InvestigationWorkspace Workspace =>
            session.Workspace;

        public InvestigationBundle Bundle =>
            session.Bundle;

        public InvestigationLoadResult LastLoad =>
            session.LastLoad;

        public bool IsLoaded =>
            session.State ==
            InvestigationViewerState.Loaded;

        public string Source =>
            session.Source;

        public int SessionFilterIndex
        {
            get
            {
                var workspace = Workspace;

                if (workspace == null ||
                    workspace.SessionFilter == null)
                {
                    return 0;
                }

                for (var index = 0;
                     index < workspace.Sessions.Count;
                     index++)
                {
                    if (ReferenceEquals(
                            workspace.Sessions[index],
                            workspace.SessionFilter))
                    {
                        return index + 1;
                    }
                }

                return 0;
            }
        }

        public bool LoadFromFile(
            string path,
            double secondsPerEntry = 0.5)
        {
            EnsureNotDisposed();

            return session.TryLoadFromFile(
                path,
                secondsPerEntry);
        }

        public void Clear()
        {
            EnsureNotDisposed();
            session.Clear();
        }

        public bool SetOutcomeFilter(
            InvestigationOutcomeFilter filter)
        {
            EnsureNotDisposed();

            var workspace = Workspace;

            if (workspace == null)
            {
                return false;
            }

            workspace.SetOutcomeFilter(filter);
            return true;
        }

        public bool SetSessionFilterIndex(int popupIndex)
        {
            EnsureNotDisposed();

            var workspace = Workspace;

            if (workspace == null)
            {
                return false;
            }

            if (popupIndex == 0)
            {
                workspace.ClearSessionFilter();
                return true;
            }

            var sessionIndex = popupIndex - 1;

            if (sessionIndex < 0 ||
                sessionIndex >= workspace.Sessions.Count)
            {
                return false;
            }

            var selectedSession =
                workspace.Sessions[sessionIndex];

            return workspace.TrySetSessionFilter(
                selectedSession.ClientIdValue,
                selectedSession.SessionIdValue);
        }

        public bool SelectVisibleEvidence(int visibleIndex)
        {
            EnsureNotDisposed();

            return Workspace != null &&
                Workspace.SelectVisibleEvidence(visibleIndex);
        }

        public bool SelectTimelineIndex(int timelineIndex)
        {
            EnsureNotDisposed();

            return Workspace != null &&
                Workspace.SelectEvidenceByTimelineIndex(
                    timelineIndex);
        }

        public bool SelectOrdinal(int ordinal)
        {
            EnsureNotDisposed();

            return Workspace != null &&
                Workspace.SelectEvidenceByOrdinal(ordinal);
        }

        public bool SeekToTick(ulong targetTick)
        {
            EnsureNotDisposed();

            return Workspace != null &&
                Workspace.Playback.SeekToTick(targetTick);
        }

        public bool Play()
        {
            EnsureNotDisposed();

            if (Workspace == null)
            {
                return false;
            }

            var started = Workspace.Playback.Play();
            Changed?.Invoke();
            return started;
        }

        public void Pause()
        {
            EnsureNotDisposed();

            if (Workspace == null)
            {
                return;
            }

            Workspace.Playback.Pause();
            Changed?.Invoke();
        }

        public bool TogglePlayback()
        {
            EnsureNotDisposed();

            if (Workspace == null)
            {
                return false;
            }

            if (Workspace.Playback.IsPlaying)
            {
                Workspace.Playback.Pause();
                Changed?.Invoke();
                return false;
            }

            var started = Workspace.Playback.Play();
            Changed?.Invoke();
            return started;
        }

        public bool StepForward()
        {
            EnsureNotDisposed();

            return Workspace != null &&
                Workspace.Playback.StepForward();
        }

        public bool StepBackward()
        {
            EnsureNotDisposed();

            return Workspace != null &&
                Workspace.Playback.StepBackward();
        }

        public void ResetPlayback()
        {
            EnsureNotDisposed();

            if (Workspace == null)
            {
                return;
            }

            Workspace.Playback.Reset();
            Changed?.Invoke();
        }

        public void SetPlaybackSpeed(double speed)
        {
            EnsureNotDisposed();

            if (Workspace == null)
            {
                return;
            }

            Workspace.Playback.SetPlaybackSpeed(speed);
            Changed?.Invoke();
        }

        public int Advance(double elapsedSeconds)
        {
            EnsureNotDisposed();

            return Workspace == null
                ? 0
                : Workspace.Playback.Advance(
                    elapsedSeconds);
        }

        public void Dispose()
        {
            if (disposed)
            {
                return;
            }

            session.LoadAttempted -= HandleLoadAttempted;
            session.StateChanged -= HandleStateChanged;

            RebindWorkspace(null);
            session.Dispose();

            disposed = true;
        }

        private void HandleLoadAttempted(
            InvestigationLoadResult result)
        {
            if (result.Succeeded)
            {
                RebindWorkspace(result.Workspace);
            }

            Changed?.Invoke();
        }

        private void HandleStateChanged()
        {
            if (ReferenceEquals(
                    observedWorkspace,
                    session.Workspace))
            {
                return;
            }

            RebindWorkspace(session.Workspace);
            Changed?.Invoke();
        }

        private void RebindWorkspace(
            InvestigationWorkspace workspace)
        {
            if (ReferenceEquals(
                    observedWorkspace,
                    workspace))
            {
                return;
            }

            if (observedWorkspace != null)
            {
                observedWorkspace.SelectionChanged -=
                    HandleWorkspaceChanged;

                observedWorkspace.FilterChanged -=
                    HandleWorkspaceChanged;
            }

            observedWorkspace = workspace;

            if (observedWorkspace != null)
            {
                observedWorkspace.SelectionChanged +=
                    HandleWorkspaceChanged;

                observedWorkspace.FilterChanged +=
                    HandleWorkspaceChanged;
            }
        }

        private void HandleWorkspaceChanged()
        {
            Changed?.Invoke();
        }

        private void EnsureNotDisposed()
        {
            if (disposed)
            {
                throw new ObjectDisposedException(
                    nameof(InvestigationViewerController));
            }
        }
    }
}
