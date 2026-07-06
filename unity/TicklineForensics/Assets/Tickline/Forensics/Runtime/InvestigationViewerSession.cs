using System;
using System.Text;

namespace Tickline.Forensics
{
    public enum InvestigationViewerState
    {
        Empty = 0,
        Loaded = 1
    }

    public sealed class InvestigationLoadResult
    {
        internal InvestigationLoadResult(
            string source,
            InvestigationBundle bundle,
            InvestigationWorkspace workspace,
            BundleValidationResult validation)
        {
            Source = source ?? string.Empty;
            Bundle = bundle;
            Workspace = workspace;
            Validation = validation;
            ErrorSummary = BuildErrorSummary(validation);
        }

        public string Source { get; }

        public InvestigationBundle Bundle { get; }

        public InvestigationWorkspace Workspace { get; }

        public BundleValidationResult Validation { get; }

        public bool Succeeded =>
            Workspace != null &&
            Validation != null &&
            Validation.IsValid;

        public string ErrorSummary { get; }

        private static string BuildErrorSummary(
            BundleValidationResult validation)
        {
            if (validation == null ||
                validation.Issues.Count == 0)
            {
                return string.Empty;
            }

            var builder = new StringBuilder();

            for (var index = 0;
                 index < validation.Issues.Count;
                 index++)
            {
                var issue = validation.Issues[index];

                if (index > 0)
                {
                    builder.AppendLine();
                }

                builder.Append(issue.Code);
                builder.Append(" at ");
                builder.Append(issue.Path);
                builder.Append(": ");
                builder.Append(issue.Message);
            }

            return builder.ToString();
        }
    }

    public sealed class InvestigationViewerSession :
        IDisposable
    {
        private InvestigationBundle bundle;
        private InvestigationWorkspace workspace;
        private string activeSource = string.Empty;
        private bool disposed;

        public event Action StateChanged;

        public event Action<InvestigationLoadResult>
            LoadAttempted;

        public InvestigationViewerState State =>
            workspace == null
                ? InvestigationViewerState.Empty
                : InvestigationViewerState.Loaded;

        public InvestigationBundle Bundle => bundle;

        public InvestigationWorkspace Workspace => workspace;

        public InvestigationLoadResult LastLoad { get; private set; }

        public string Source => activeSource;

        public bool TryLoadFromFile(
            string path,
            double secondsPerEntry = 0.5)
        {
            EnsureNotDisposed();

            var load =
                InvestigationBundleLoader.LoadFromFile(path);

            return CommitLoad(
                path ?? string.Empty,
                load,
                secondsPerEntry);
        }

        public bool TryLoadFromJson(
            string json,
            string source = "memory",
            double secondsPerEntry = 0.5)
        {
            EnsureNotDisposed();

            var resolvedSource =
                string.IsNullOrWhiteSpace(source)
                    ? "memory"
                    : source;

            var load =
                InvestigationBundleLoader.LoadFromJson(json);

            return CommitLoad(
                resolvedSource,
                load,
                secondsPerEntry);
        }

        public void Clear()
        {
            EnsureNotDisposed();

            if (workspace == null &&
                bundle == null)
            {
                return;
            }

            ResetActiveWorkspace();
            StateChanged?.Invoke();
        }

        public void Dispose()
        {
            if (disposed)
            {
                return;
            }

            ResetActiveWorkspace();
            disposed = true;
        }

        private bool CommitLoad(
            string source,
            BundleLoadResult load,
            double secondsPerEntry)
        {
            if (load == null)
            {
                throw new ArgumentNullException(
                    nameof(load));
            }

            if (load.Validation == null ||
                !load.Validation.IsValid ||
                load.Bundle == null)
            {
                LastLoad =
                    new InvestigationLoadResult(
                        source,
                        load.Bundle,
                        null,
                        load.Validation);

                LoadAttempted?.Invoke(LastLoad);
                return false;
            }

            var workspaceResult =
                InvestigationWorkspaceBuilder.Build(
                    load.Bundle,
                    secondsPerEntry);

            if (!workspaceResult.IsValid)
            {
                LastLoad =
                    new InvestigationLoadResult(
                        source,
                        load.Bundle,
                        null,
                        workspaceResult.Validation);

                LoadAttempted?.Invoke(LastLoad);
                return false;
            }

            var previousWorkspace = workspace;

            bundle = load.Bundle;
            workspace = workspaceResult.Workspace;
            activeSource = source;

            LastLoad =
                new InvestigationLoadResult(
                    source,
                    bundle,
                    workspace,
                    workspaceResult.Validation);

            previousWorkspace?.Dispose();

            LoadAttempted?.Invoke(LastLoad);
            StateChanged?.Invoke();

            return true;
        }

        private void ResetActiveWorkspace()
        {
            workspace?.Dispose();
            workspace = null;
            bundle = null;
            activeSource = string.Empty;
        }

        private void EnsureNotDisposed()
        {
            if (disposed)
            {
                throw new ObjectDisposedException(
                    nameof(InvestigationViewerSession));
            }
        }
    }
}
