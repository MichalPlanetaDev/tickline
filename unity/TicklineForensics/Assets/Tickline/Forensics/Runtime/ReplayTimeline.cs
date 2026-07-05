using System;
using System.Collections.Generic;
using System.Globalization;

namespace Tickline.Forensics
{
    public sealed class ReplayTimelineEntry
    {
        internal ReplayTimelineEntry(
            int index,
            InvestigationEvidenceRecord record)
        {
            Index = index;
            Ordinal = record.ordinal;
            ClientId = record.clientId;
            SessionId = record.sessionId;
            SessionSequence = ulong.Parse(
                record.sessionSequence,
                NumberStyles.None,
                CultureInfo.InvariantCulture);
            TargetTick = ulong.Parse(
                record.targetTick,
                NumberStyles.None,
                CultureInfo.InvariantCulture);
            CommandType = record.commandType;
            Outcome = record.outcome;
            RejectionCode = record.rejectionCode;
            PreviousDigest = record.previousDigest;
            RecordDigest = record.recordDigest;
        }

        public int Index { get; }

        public int Ordinal { get; }

        public string ClientId { get; }

        public string SessionId { get; }

        public ulong SessionSequence { get; }

        public ulong TargetTick { get; }

        public string CommandType { get; }

        public string Outcome { get; }

        public string RejectionCode { get; }

        public string PreviousDigest { get; }

        public string RecordDigest { get; }

        public bool IsAccepted =>
            string.Equals(
                Outcome,
                "accepted",
                StringComparison.Ordinal);

        public bool IsRejected =>
            string.Equals(
                Outcome,
                "rejected",
                StringComparison.Ordinal);
    }

    public sealed class ReplayTimeline
    {
        private readonly List<ReplayTimelineEntry> entries;

        internal ReplayTimeline(
            InvestigationBundle bundle,
            List<ReplayTimelineEntry> entries)
        {
            ArchiveDigest = bundle.archiveDigest;
            InitialHeadDigest = bundle.initialHeadDigest;
            TrustedHeadDigest = bundle.trustedHeadDigest;
            Replay = bundle.replay;
            this.entries = entries;
        }

        public string ArchiveDigest { get; }

        public string InitialHeadDigest { get; }

        public string TrustedHeadDigest { get; }

        public ReplaySummary Replay { get; }

        public int Count => entries.Count;

        public IReadOnlyList<ReplayTimelineEntry> Entries => entries;

        public ReplayTimelineEntry this[int index] => entries[index];

        public int FindNearestIndexToTick(ulong targetTick)
        {
            if (entries.Count == 0)
            {
                return -1;
            }

            var nearestIndex = 0;
            var nearestDistance =
                Distance(entries[0].TargetTick, targetTick);

            for (var index = 1; index < entries.Count; index++)
            {
                var distance =
                    Distance(entries[index].TargetTick, targetTick);

                if (distance < nearestDistance)
                {
                    nearestIndex = index;
                    nearestDistance = distance;
                }
            }

            return nearestIndex;
        }

        private static ulong Distance(ulong left, ulong right)
        {
            return left >= right
                ? left - right
                : right - left;
        }
    }

    public sealed class ReplayTimelineBuildResult
    {
        public ReplayTimelineBuildResult(
            ReplayTimeline timeline,
            BundleValidationResult validation)
        {
            Timeline = timeline;
            Validation = validation;
        }

        public ReplayTimeline Timeline { get; }

        public BundleValidationResult Validation { get; }

        public bool IsValid =>
            Timeline != null &&
            Validation != null &&
            Validation.IsValid;
    }

    public static class ReplayTimelineBuilder
    {
        public static ReplayTimelineBuildResult Build(
            InvestigationBundle bundle)
        {
            var validation =
                InvestigationBundleValidator.Validate(bundle);

            if (!validation.IsValid)
            {
                return new ReplayTimelineBuildResult(
                    null,
                    validation);
            }

            var evidence = bundle.evidence ??
                Array.Empty<InvestigationEvidenceRecord>();

            var entries =
                new List<ReplayTimelineEntry>(evidence.Length);

            for (var index = 0; index < evidence.Length; index++)
            {
                entries.Add(
                    new ReplayTimelineEntry(
                        index,
                        evidence[index]));
            }

            return new ReplayTimelineBuildResult(
                new ReplayTimeline(bundle, entries),
                validation);
        }
    }
}
