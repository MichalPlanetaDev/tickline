using System;
using System.Collections.Generic;
using System.Globalization;

namespace Tickline.Forensics
{
    public static class InvestigationBundleValidator
    {
        public const int SupportedSchemaVersion = 1;

        public static BundleValidationResult Validate(
            InvestigationBundle bundle)
        {
            var result = new BundleValidationResult();

            if (bundle == null)
            {
                result.Add(
                    "bundle_missing",
                    "$",
                    "The investigation bundle is missing.");
                return result;
            }

            if (bundle.schemaVersion != SupportedSchemaVersion)
            {
                result.Add(
                    "schema_unsupported",
                    "$.schemaVersion",
                    "The bundle schema version is not supported.");
            }

            ValidateDigest(
                bundle.archiveDigest,
                "$.archiveDigest",
                result);

            ValidateDigest(
                bundle.initialHeadDigest,
                "$.initialHeadDigest",
                result);

            ValidateDigest(
                bundle.trustedHeadDigest,
                "$.trustedHeadDigest",
                result);

            ValidateTimestamp(bundle.importedAtUtc, result);

            var sessions = bundle.sessions ??
                Array.Empty<InvestigationSession>();

            var evidence = bundle.evidence ??
                Array.Empty<InvestigationEvidenceRecord>();

            ValidateSessions(sessions, result);
            ValidateEvidence(
                evidence,
                sessions,
                bundle.initialHeadDigest,
                bundle.trustedHeadDigest,
                result);

            ValidateReplay(bundle.replay, evidence, result);

            return result;
        }

        private static void ValidateSessions(
            InvestigationSession[] sessions,
            BundleValidationResult result)
        {
            var identities = new HashSet<string>(StringComparer.Ordinal);

            for (var index = 0; index < sessions.Length; index++)
            {
                var session = sessions[index];
                var path = $"$.sessions[{index}]";

                if (session == null)
                {
                    result.Add(
                        "session_missing",
                        path,
                        "The session entry is null.");
                    continue;
                }

                RequireText(
                    session.clientId,
                    path + ".clientId",
                    result);

                RequireText(
                    session.sessionId,
                    path + ".sessionId",
                    result);

                ValidateUInt64(
                    session.lastCommittedSequence,
                    path + ".lastCommittedSequence",
                    result);

                ValidateUInt64(
                    session.firstTargetTick,
                    path + ".firstTargetTick",
                    result);

                ValidateUInt64(
                    session.lastTargetTick,
                    path + ".lastTargetTick",
                    result);

                if (session.acceptedCommands < 0)
                {
                    result.Add(
                        "count_negative",
                        path + ".acceptedCommands",
                        "Accepted-command count cannot be negative.");
                }

                if (session.rejectedCommands < 0)
                {
                    result.Add(
                        "count_negative",
                        path + ".rejectedCommands",
                        "Rejected-command count cannot be negative.");
                }

                var identity = session.clientId + "\n" + session.sessionId;

                if (!identities.Add(identity))
                {
                    result.Add(
                        "session_duplicate",
                        path,
                        "The client and session identity is duplicated.");
                }
            }
        }

        private static void ValidateEvidence(
            InvestigationEvidenceRecord[] evidence,
            InvestigationSession[] sessions,
            string initialHeadDigest,
            string trustedHeadDigest,
            BundleValidationResult result)
        {
            var sessionIdentities =
                new HashSet<string>(StringComparer.Ordinal);

            foreach (var session in sessions)
            {
                if (session != null)
                {
                    sessionIdentities.Add(
                        session.clientId + "\n" + session.sessionId);
                }
            }

            var expectedPrevious = initialHeadDigest;

            for (var index = 0; index < evidence.Length; index++)
            {
                var record = evidence[index];
                var path = $"$.evidence[{index}]";

                if (record == null)
                {
                    result.Add(
                        "evidence_missing",
                        path,
                        "The evidence entry is null.");
                    continue;
                }

                if (record.ordinal != index)
                {
                    result.Add(
                        "evidence_ordinal_invalid",
                        path + ".ordinal",
                        "Evidence ordinals must be contiguous and zero-based.");
                }

                RequireText(record.clientId, path + ".clientId", result);
                RequireText(record.sessionId, path + ".sessionId", result);
                RequireText(record.commandType, path + ".commandType", result);

                ValidateUInt64(
                    record.sessionSequence,
                    path + ".sessionSequence",
                    result);

                ValidateUInt64(
                    record.targetTick,
                    path + ".targetTick",
                    result);

                ValidateDigest(
                    record.previousDigest,
                    path + ".previousDigest",
                    result);

                ValidateDigest(
                    record.recordDigest,
                    path + ".recordDigest",
                    result);

                if (record.outcome != "accepted" &&
                    record.outcome != "rejected")
                {
                    result.Add(
                        "evidence_outcome_invalid",
                        path + ".outcome",
                        "Evidence outcome must be accepted or rejected.");
                }

                var identity = record.clientId + "\n" + record.sessionId;

                if (!sessionIdentities.Contains(identity))
                {
                    result.Add(
                        "evidence_session_unknown",
                        path,
                        "Evidence references an unknown session.");
                }

                if (!string.Equals(
                        record.previousDigest,
                        expectedPrevious,
                        StringComparison.Ordinal))
                {
                    result.Add(
                        "evidence_chain_broken",
                        path + ".previousDigest",
                        "The previous digest does not match the chain head.");
                }

                expectedPrevious = record.recordDigest;
            }

            var expectedHead = evidence.Length == 0
                ? initialHeadDigest
                : evidence[evidence.Length - 1]?.recordDigest;

            if (!string.Equals(
                    trustedHeadDigest,
                    expectedHead,
                    StringComparison.Ordinal))
            {
                result.Add(
                    "trusted_head_mismatch",
                    "$.trustedHeadDigest",
                    "The trusted head does not match the final evidence record.");
            }
        }

        private static void ValidateReplay(
            ReplaySummary replay,
            InvestigationEvidenceRecord[] evidence,
            BundleValidationResult result)
        {
            if (replay == null)
            {
                result.Add(
                    "replay_missing",
                    "$.replay",
                    "Replay summary is missing.");
                return;
            }

            ValidateUInt64(
                replay.finalTick,
                "$.replay.finalTick",
                result);

            ValidateUInt64(
                replay.finalWorldFingerprint,
                "$.replay.finalWorldFingerprint",
                result);

            var accepted = 0;
            var rejected = 0;

            foreach (var record in evidence)
            {
                if (record == null)
                {
                    continue;
                }

                if (record.outcome == "accepted")
                {
                    accepted++;
                }
                else if (record.outcome == "rejected")
                {
                    rejected++;
                }
            }

            if (replay.acceptedCommands != accepted)
            {
                result.Add(
                    "replay_accepted_count_mismatch",
                    "$.replay.acceptedCommands",
                    "Replay accepted-command count does not match evidence.");
            }

            if (replay.rejectedCommands != rejected)
            {
                result.Add(
                    "replay_rejected_count_mismatch",
                    "$.replay.rejectedCommands",
                    "Replay rejected-command count does not match evidence.");
            }
        }

        private static void ValidateTimestamp(
            string value,
            BundleValidationResult result)
        {
            if (!DateTimeOffset.TryParse(
                    value,
                    CultureInfo.InvariantCulture,
                    DateTimeStyles.AssumeUniversal |
                    DateTimeStyles.AdjustToUniversal,
                    out _))
            {
                result.Add(
                    "timestamp_invalid",
                    "$.importedAtUtc",
                    "The import timestamp is not valid ISO-8601.");
            }
        }

        private static void ValidateUInt64(
            string value,
            string path,
            BundleValidationResult result)
        {
            if (string.IsNullOrEmpty(value) ||
                !ulong.TryParse(
                    value,
                    NumberStyles.None,
                    CultureInfo.InvariantCulture,
                    out _))
            {
                result.Add(
                    "uint64_invalid",
                    path,
                    "The value must be an unsigned 64-bit decimal string.");
            }
        }

        private static void ValidateDigest(
            string value,
            string path,
            BundleValidationResult result)
        {
            if (value == null || value.Length != 64)
            {
                result.Add(
                    "digest_invalid",
                    path,
                    "A SHA-256 digest must contain 64 lowercase hex characters.");
                return;
            }

            foreach (var character in value)
            {
                var isDigit = character >= '0' && character <= '9';
                var isLowerHex = character >= 'a' && character <= 'f';

                if (!isDigit && !isLowerHex)
                {
                    result.Add(
                        "digest_invalid",
                        path,
                        "A SHA-256 digest must contain 64 lowercase hex characters.");
                    return;
                }
            }
        }

        private static void RequireText(
            string value,
            string path,
            BundleValidationResult result)
        {
            if (string.IsNullOrWhiteSpace(value))
            {
                result.Add(
                    "text_missing",
                    path,
                    "The value cannot be empty.");
            }
        }
    }
}
