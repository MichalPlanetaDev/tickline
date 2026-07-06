using System;

namespace Tickline.Forensics
{
    [Serializable]
    public sealed class InvestigationBundle
    {
        public int schemaVersion;
        public string archiveDigest = string.Empty;
        public string initialHeadDigest = string.Empty;
        public string trustedHeadDigest = string.Empty;
        public string importedAtUtc = string.Empty;
        public InvestigationSession[] sessions = Array.Empty<InvestigationSession>();
        public InvestigationEvidenceRecord[] evidence = Array.Empty<InvestigationEvidenceRecord>();
        public ReplaySummary replay = new ReplaySummary();
    }

    [Serializable]
    public sealed class InvestigationSession
    {
        public string clientId = string.Empty;
        public string sessionId = string.Empty;
        public string lastCommittedSequence = "0";
        public string firstTargetTick = "0";
        public string lastTargetTick = "0";
        public int acceptedCommands;
        public int rejectedCommands;
    }

    [Serializable]
    public sealed class InvestigationEvidenceRecord
    {
        public int ordinal;
        public string clientId = string.Empty;
        public string sessionId = string.Empty;
        public string sessionSequence = "0";
        public string targetTick = "0";
        public string commandType = string.Empty;
        public string outcome = string.Empty;
        public string rejectionCode = string.Empty;
        public string previousDigest = string.Empty;
        public string recordDigest = string.Empty;
    }

    [Serializable]
    public sealed class ReplaySummary
    {
        public bool verified;
        public string finalTick = "0";
        public string finalWorldFingerprint = "0";
        public int acceptedCommands;
        public int rejectedCommands;
    }
}
