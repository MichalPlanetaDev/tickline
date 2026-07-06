using System.Collections.Generic;

namespace Tickline.Forensics
{
    public sealed class BundleValidationIssue
    {
        public BundleValidationIssue(string code, string path, string message)
        {
            Code = code;
            Path = path;
            Message = message;
        }

        public string Code { get; }
        public string Path { get; }
        public string Message { get; }
    }

    public sealed class BundleValidationResult
    {
        private readonly List<BundleValidationIssue> issues =
            new List<BundleValidationIssue>();

        public bool IsValid => issues.Count == 0;

        public IReadOnlyList<BundleValidationIssue> Issues => issues;

        internal void Add(string code, string path, string message)
        {
            issues.Add(new BundleValidationIssue(code, path, message));
        }
    }

    public sealed class BundleLoadResult
    {
        public BundleLoadResult(
            InvestigationBundle bundle,
            BundleValidationResult validation)
        {
            Bundle = bundle;
            Validation = validation;
        }

        public InvestigationBundle Bundle { get; }

        public BundleValidationResult Validation { get; }
    }
}
