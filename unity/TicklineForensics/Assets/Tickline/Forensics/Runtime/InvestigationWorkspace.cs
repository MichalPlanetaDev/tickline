using System;
using System.Collections.Generic;
using System.Globalization;

namespace Tickline.Forensics
{
    public enum InvestigationOutcomeFilter
    {
        All = 0,
        Accepted = 1,
        Rejected = 2
    }

    public enum InvestigationIntegrityStatus
    {
        ReplayVerified = 0,
        ReplayNotVerified = 1
    }

    internal readonly struct InvestigationSessionKey :
        IEquatable<InvestigationSessionKey>
    {
        public InvestigationSessionKey(
            ulong clientId,
            ulong sessionId)
        {
            ClientId = clientId;
            SessionId = sessionId;
        }

        public ulong ClientId { get; }

        public ulong SessionId { get; }

        public bool Equals(InvestigationSessionKey other)
        {
            return ClientId == other.ClientId &&
                SessionId == other.SessionId;
        }

        public override bool Equals(object obj)
        {
            return obj is InvestigationSessionKey other &&
                Equals(other);
        }

        public override int GetHashCode()
        {
            unchecked
            {
                return
                    (ClientId.GetHashCode() * 397) ^
                    SessionId.GetHashCode();
            }
        }
    }

    public sealed class InvestigationSessionInspection
    {
        internal InvestigationSessionInspection(
            InvestigationSession session)
        {
            ClientId = session.clientId;
            SessionId = session.sessionId;

            ClientIdValue = ParseUnsigned(
                session.clientId);

            SessionIdValue = ParseUnsigned(
                session.sessionId);

            LastCommittedSequence = ParseUnsigned(
                session.lastCommittedSequence);

            FirstTargetTick = ParseUnsigned(
                session.firstTargetTick);

            LastTargetTick = ParseUnsigned(
                session.lastTargetTick);

            AcceptedCommands = session.acceptedCommands;
            RejectedCommands = session.rejectedCommands;
        }

        public string ClientId { get; }

        public string SessionId { get; }

        public ulong ClientIdValue { get; }

        public ulong SessionIdValue { get; }

        public ulong LastCommittedSequence { get; }

        public ulong FirstTargetTick { get; }

        public ulong LastTargetTick { get; }

        public int AcceptedCommands { get; }

        public int RejectedCommands { get; }

        public long TotalCommands =>
            (long)AcceptedCommands + RejectedCommands;

        internal InvestigationSessionKey Key =>
            new InvestigationSessionKey(
                ClientIdValue,
                SessionIdValue);

        private static ulong ParseUnsigned(string value)
        {
            return ulong.Parse(
                value,
                NumberStyles.None,
                CultureInfo.InvariantCulture);
        }
    }

    public sealed class InvestigationEvidenceInspection
    {
        internal InvestigationEvidenceInspection(
            ReplayTimeline timeline,
            ReplayTimelineEntry entry,
            InvestigationSessionInspection session)
        {
            TimelineEntry = entry;
            Session = session;

            ExpectedPreviousDigest =
                entry.Index == 0
                    ? timeline.InitialHeadDigest
                    : timeline[entry.Index - 1].RecordDigest;

            PreviousDigestMatches =
                string.Equals(
                    entry.PreviousDigest,
                    ExpectedPreviousDigest,
                    StringComparison.Ordinal);

            IsFinalEvidence =
                entry.Index == timeline.Count - 1;

            CompletesTrustedHead =
                IsFinalEvidence &&
                string.Equals(
                    entry.RecordDigest,
                    timeline.TrustedHeadDigest,
                    StringComparison.Ordinal);
        }

        public ReplayTimelineEntry TimelineEntry { get; }

        public InvestigationSessionInspection Session { get; }

        public int TimelineIndex =>
            TimelineEntry.Index;

        public int Ordinal =>
            TimelineEntry.Ordinal;

        public string ClientId =>
            TimelineEntry.ClientId;

        public string SessionId =>
            TimelineEntry.SessionId;

        public ulong SessionSequence =>
            TimelineEntry.SessionSequence;

        public ulong TargetTick =>
            TimelineEntry.TargetTick;

        public string CommandType =>
            TimelineEntry.CommandType;

        public string Outcome =>
            TimelineEntry.Outcome;

        public string RejectionCode =>
            TimelineEntry.RejectionCode;

        public string PreviousDigest =>
            TimelineEntry.PreviousDigest;

        public string RecordDigest =>
            TimelineEntry.RecordDigest;

        public string ExpectedPreviousDigest { get; }

        public bool IsAccepted =>
            TimelineEntry.IsAccepted;

        public bool IsRejected =>
            TimelineEntry.IsRejected;

        public bool PreviousDigestMatches { get; }

        public bool IsFinalEvidence { get; }

        public bool CompletesTrustedHead { get; }
    }

    public sealed class InvestigationIntegritySummary
    {
        internal InvestigationIntegritySummary(
            ReplayTimeline timeline,
            IReadOnlyList<InvestigationEvidenceInspection> evidence)
        {
            StructuralValidationPassed = true;
            EvidenceChainContinuous = true;

            for (var index = 0;
                 index < evidence.Count;
                 index++)
            {
                if (!evidence[index].PreviousDigestMatches)
                {
                    EvidenceChainContinuous = false;
                    break;
                }
            }

            if (evidence.Count == 0)
            {
                TrustedHeadMatches =
                    string.Equals(
                        timeline.InitialHeadDigest,
                        timeline.TrustedHeadDigest,
                        StringComparison.Ordinal);
            }
            else
            {
                TrustedHeadMatches =
                    evidence[evidence.Count - 1]
                        .CompletesTrustedHead;
            }

            ReplayVerified =
                timeline.Replay != null &&
                timeline.Replay.verified;

            Status =
                EvidenceChainContinuous &&
                TrustedHeadMatches &&
                ReplayVerified
                    ? InvestigationIntegrityStatus
                        .ReplayVerified
                    : InvestigationIntegrityStatus
                        .ReplayNotVerified;
        }

        public bool StructuralValidationPassed { get; }

        public bool EvidenceChainContinuous { get; }

        public bool TrustedHeadMatches { get; }

        public bool ReplayVerified { get; }

        public InvestigationIntegrityStatus Status { get; }

        public string StatusCode =>
            Status ==
            InvestigationIntegrityStatus.ReplayVerified
                ? "replay_verified"
                : "replay_not_verified";
    }

    public sealed class InvestigationWorkspaceBuildResult
    {
        public InvestigationWorkspaceBuildResult(
            InvestigationWorkspace workspace,
            BundleValidationResult validation)
        {
            Workspace = workspace;
            Validation = validation;
        }

        public InvestigationWorkspace Workspace { get; }

        public BundleValidationResult Validation { get; }

        public bool IsValid =>
            Workspace != null &&
            Validation != null &&
            Validation.IsValid;
    }

    public sealed class InvestigationWorkspace :
        IDisposable
    {
        private readonly List<InvestigationSessionInspection>
            sessions;

        private readonly List<InvestigationEvidenceInspection>
            evidence;

        private readonly List<InvestigationEvidenceInspection>
            visibleEvidence;

        private InvestigationOutcomeFilter outcomeFilter;
        private InvestigationSessionInspection sessionFilter;
        private bool disposed;

        internal InvestigationWorkspace(
            ReplayTimeline timeline,
            ReplayPlaybackController playback,
            List<InvestigationSessionInspection> sessions,
            List<InvestigationEvidenceInspection> evidence)
        {
            Timeline = timeline;
            Playback = playback;
            this.sessions = sessions;
            this.evidence = evidence;
            visibleEvidence =
                new List<InvestigationEvidenceInspection>(
                    evidence.Count);

            Integrity =
                new InvestigationIntegritySummary(
                    timeline,
                    evidence);

            outcomeFilter =
                InvestigationOutcomeFilter.All;

            Playback.PositionChanged +=
                HandlePlaybackPositionChanged;

            RebuildVisibleEvidence();
        }

        public event Action SelectionChanged;

        public event Action FilterChanged;

        public ReplayTimeline Timeline { get; }

        public ReplayPlaybackController Playback { get; }

        public InvestigationIntegritySummary Integrity { get; }

        public IReadOnlyList<InvestigationSessionInspection>
            Sessions => sessions;

        public IReadOnlyList<InvestigationEvidenceInspection>
            Evidence => evidence;

        public IReadOnlyList<InvestigationEvidenceInspection>
            VisibleEvidence => visibleEvidence;

        public InvestigationOutcomeFilter OutcomeFilter =>
            outcomeFilter;

        public InvestigationSessionInspection SessionFilter =>
            sessionFilter;

        public InvestigationEvidenceInspection SelectedEvidence
        {
            get
            {
                var index = Playback.CurrentIndex;

                return index >= 0 &&
                    index < evidence.Count
                        ? evidence[index]
                        : null;
            }
        }

        public InvestigationSessionInspection SelectedSession =>
            SelectedEvidence?.Session;

        public int SelectedVisibleIndex
        {
            get
            {
                var selected = SelectedEvidence;

                if (selected == null)
                {
                    return -1;
                }

                for (var index = 0;
                     index < visibleEvidence.Count;
                     index++)
                {
                    if (visibleEvidence[index].TimelineIndex ==
                        selected.TimelineIndex)
                    {
                        return index;
                    }
                }

                return -1;
            }
        }

        public bool IsSelectedEvidenceVisible =>
            SelectedVisibleIndex >= 0;

        public void SetOutcomeFilter(
            InvestigationOutcomeFilter filter)
        {
            if (!Enum.IsDefined(
                    typeof(InvestigationOutcomeFilter),
                    filter))
            {
                throw new ArgumentOutOfRangeException(
                    nameof(filter));
            }

            if (outcomeFilter == filter)
            {
                return;
            }

            outcomeFilter = filter;
            RebuildVisibleEvidence();
            FilterChanged?.Invoke();
        }

        public bool TrySetSessionFilter(
            ulong clientId,
            ulong sessionId)
        {
            for (var index = 0;
                 index < sessions.Count;
                 index++)
            {
                var candidate = sessions[index];

                if (candidate.ClientIdValue == clientId &&
                    candidate.SessionIdValue == sessionId)
                {
                    if (ReferenceEquals(
                            sessionFilter,
                            candidate))
                    {
                        return true;
                    }

                    sessionFilter = candidate;
                    RebuildVisibleEvidence();
                    FilterChanged?.Invoke();
                    return true;
                }
            }

            return false;
        }

        public bool TrySetSessionFilter(
            string clientId,
            string sessionId)
        {
            if (!ulong.TryParse(
                    clientId,
                    NumberStyles.None,
                    CultureInfo.InvariantCulture,
                    out var clientValue))
            {
                return false;
            }

            if (!ulong.TryParse(
                    sessionId,
                    NumberStyles.None,
                    CultureInfo.InvariantCulture,
                    out var sessionValue))
            {
                return false;
            }

            return TrySetSessionFilter(
                clientValue,
                sessionValue);
        }

        public void ClearSessionFilter()
        {
            if (sessionFilter == null)
            {
                return;
            }

            sessionFilter = null;
            RebuildVisibleEvidence();
            FilterChanged?.Invoke();
        }

        public bool SelectEvidenceByTimelineIndex(
            int timelineIndex)
        {
            return Playback.Seek(timelineIndex);
        }

        public bool SelectEvidenceByOrdinal(int ordinal)
        {
            for (var index = 0;
                 index < evidence.Count;
                 index++)
            {
                if (evidence[index].Ordinal == ordinal)
                {
                    return Playback.Seek(
                        evidence[index].TimelineIndex);
                }
            }

            return false;
        }

        public bool SelectVisibleEvidence(int visibleIndex)
        {
            if (visibleIndex < 0 ||
                visibleIndex >= visibleEvidence.Count)
            {
                return false;
            }

            return Playback.Seek(
                visibleEvidence[visibleIndex]
                    .TimelineIndex);
        }

        public void Dispose()
        {
            if (disposed)
            {
                return;
            }

            Playback.PositionChanged -=
                HandlePlaybackPositionChanged;

            disposed = true;
        }

        private void HandlePlaybackPositionChanged(int index)
        {
            SelectionChanged?.Invoke();
        }

        private void RebuildVisibleEvidence()
        {
            visibleEvidence.Clear();

            for (var index = 0;
                 index < evidence.Count;
                 index++)
            {
                var item = evidence[index];

                if (!MatchesOutcomeFilter(item))
                {
                    continue;
                }

                if (sessionFilter != null &&
                    !ReferenceEquals(
                        item.Session,
                        sessionFilter))
                {
                    continue;
                }

                visibleEvidence.Add(item);
            }
        }

        private bool MatchesOutcomeFilter(
            InvestigationEvidenceInspection item)
        {
            switch (outcomeFilter)
            {
                case InvestigationOutcomeFilter.All:
                    return true;

                case InvestigationOutcomeFilter.Accepted:
                    return item.IsAccepted;

                case InvestigationOutcomeFilter.Rejected:
                    return item.IsRejected;

                default:
                    return false;
            }
        }
    }

    public static class InvestigationWorkspaceBuilder
    {
        public static InvestigationWorkspaceBuildResult Build(
            InvestigationBundle bundle,
            double secondsPerEntry = 0.5)
        {
            var timelineResult =
                ReplayTimelineBuilder.Build(bundle);

            if (!timelineResult.IsValid)
            {
                return new InvestigationWorkspaceBuildResult(
                    null,
                    timelineResult.Validation);
            }

            var sessionSource =
                bundle.sessions ??
                Array.Empty<InvestigationSession>();

            var sessions =
                new List<InvestigationSessionInspection>(
                    sessionSource.Length);

            var sessionMap =
                new Dictionary<
                    InvestigationSessionKey,
                    InvestigationSessionInspection>();

            for (var index = 0;
                 index < sessionSource.Length;
                 index++)
            {
                var inspection =
                    new InvestigationSessionInspection(
                        sessionSource[index]);

                sessions.Add(inspection);
                sessionMap.Add(
                    inspection.Key,
                    inspection);
            }

            sessions.Sort(
                CompareSessions);

            var timeline = timelineResult.Timeline;

            var evidence =
                new List<InvestigationEvidenceInspection>(
                    timeline.Count);

            for (var index = 0;
                 index < timeline.Count;
                 index++)
            {
                var entry = timeline[index];

                var key =
                    new InvestigationSessionKey(
                        ulong.Parse(
                            entry.ClientId,
                            NumberStyles.None,
                            CultureInfo.InvariantCulture),
                        ulong.Parse(
                            entry.SessionId,
                            NumberStyles.None,
                            CultureInfo.InvariantCulture));

                var session = sessionMap[key];

                evidence.Add(
                    new InvestigationEvidenceInspection(
                        timeline,
                        entry,
                        session));
            }

            var playback =
                new ReplayPlaybackController(
                    timeline,
                    secondsPerEntry);

            return new InvestigationWorkspaceBuildResult(
                new InvestigationWorkspace(
                    timeline,
                    playback,
                    sessions,
                    evidence),
                timelineResult.Validation);
        }

        private static int CompareSessions(
            InvestigationSessionInspection left,
            InvestigationSessionInspection right)
        {
            var clientComparison =
                left.ClientIdValue.CompareTo(
                    right.ClientIdValue);

            return clientComparison != 0
                ? clientComparison
                : left.SessionIdValue.CompareTo(
                    right.SessionIdValue);
        }
    }
}
