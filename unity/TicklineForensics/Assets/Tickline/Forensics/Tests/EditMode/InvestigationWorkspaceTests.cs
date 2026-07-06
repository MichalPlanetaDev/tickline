using System;
using System.IO;
using NUnit.Framework;
using UnityEngine;

namespace Tickline.Forensics.Tests
{
    public sealed class InvestigationWorkspaceTests
    {
        [Test]
        public void NativeBundleBuildsVerifiedWorkspace()
        {
            var result = BuildWorkspace();

            Assert.That(result.IsValid, Is.True);

            using (var workspace = result.Workspace)
            {
                Assert.That(
                    workspace.Sessions.Count,
                    Is.EqualTo(1));

                Assert.That(
                    workspace.Evidence.Count,
                    Is.EqualTo(4));

                Assert.That(
                    workspace.VisibleEvidence.Count,
                    Is.EqualTo(4));

                Assert.That(
                    workspace.SelectedEvidence.Ordinal,
                    Is.EqualTo(0));

                Assert.That(
                    workspace.Integrity
                        .StructuralValidationPassed,
                    Is.True);

                Assert.That(
                    workspace.Integrity
                        .EvidenceChainContinuous,
                    Is.True);

                Assert.That(
                    workspace.Integrity
                        .TrustedHeadMatches,
                    Is.True);

                Assert.That(
                    workspace.Integrity.ReplayVerified,
                    Is.True);

                Assert.That(
                    workspace.Integrity.Status,
                    Is.EqualTo(
                        InvestigationIntegrityStatus
                            .ReplayVerified));

                Assert.That(
                    workspace.Integrity.StatusCode,
                    Is.EqualTo("replay_verified"));
            }
        }

        [Test]
        public void SessionInspectionPreservesNativeSummary()
        {
            using (var workspace =
                BuildWorkspace().Workspace)
            {
                var session = workspace.Sessions[0];

                Assert.That(
                    session.ClientIdValue,
                    Is.EqualTo(7UL));

                Assert.That(
                    session.SessionIdValue,
                    Is.EqualTo(11UL));

                Assert.That(
                    session.LastCommittedSequence,
                    Is.EqualTo(2UL));

                Assert.That(
                    session.FirstTargetTick,
                    Is.EqualTo(2UL));

                Assert.That(
                    session.LastTargetTick,
                    Is.EqualTo(3UL));

                Assert.That(
                    session.AcceptedCommands,
                    Is.EqualTo(2));

                Assert.That(
                    session.RejectedCommands,
                    Is.EqualTo(2));

                Assert.That(
                    session.TotalCommands,
                    Is.EqualTo(4L));
            }
        }

        [Test]
        public void EvidenceInspectionExposesIntegrityContext()
        {
            using (var workspace =
                BuildWorkspace().Workspace)
            {
                var first = workspace.Evidence[0];
                var last =
                    workspace.Evidence[
                        workspace.Evidence.Count - 1];

                Assert.That(
                    first.PreviousDigestMatches,
                    Is.True);

                Assert.That(
                    first.ExpectedPreviousDigest,
                    Is.EqualTo(
                        workspace.Timeline
                            .InitialHeadDigest));

                Assert.That(first.CommandType,
                    Is.EqualTo("set_velocity"));

                Assert.That(first.IsAccepted, Is.True);
                Assert.That(first.RejectionCode,
                    Is.EqualTo("none"));

                Assert.That(last.IsFinalEvidence, Is.True);
                Assert.That(
                    last.CompletesTrustedHead,
                    Is.True);

                Assert.That(
                    last.RecordDigest,
                    Is.EqualTo(
                        workspace.Timeline
                            .TrustedHeadDigest));
            }
        }

        [Test]
        public void OutcomeFiltersPreserveTimelineOrder()
        {
            using (var workspace =
                BuildWorkspace().Workspace)
            {
                var filterEvents = 0;

                workspace.FilterChanged +=
                    () => filterEvents++;

                workspace.SetOutcomeFilter(
                    InvestigationOutcomeFilter
                        .Rejected);

                Assert.That(
                    workspace.VisibleEvidence.Count,
                    Is.EqualTo(2));

                Assert.That(
                    workspace.VisibleEvidence[0].Ordinal,
                    Is.EqualTo(1));

                Assert.That(
                    workspace.VisibleEvidence[1].Ordinal,
                    Is.EqualTo(2));

                Assert.That(
                    workspace.VisibleEvidence[0]
                        .IsRejected,
                    Is.True);

                workspace.SetOutcomeFilter(
                    InvestigationOutcomeFilter
                        .Accepted);

                Assert.That(
                    workspace.VisibleEvidence.Count,
                    Is.EqualTo(2));

                Assert.That(
                    workspace.VisibleEvidence[0].Ordinal,
                    Is.EqualTo(0));

                Assert.That(
                    workspace.VisibleEvidence[1].Ordinal,
                    Is.EqualTo(3));

                Assert.That(filterEvents, Is.EqualTo(2));

                Assert.Throws<
                    ArgumentOutOfRangeException>(
                    () => workspace.SetOutcomeFilter(
                        (InvestigationOutcomeFilter)99));
            }
        }

        [Test]
        public void SessionFilterUsesExactIdentity()
        {
            using (var workspace =
                BuildWorkspace().Workspace)
            {
                Assert.That(
                    workspace.TrySetSessionFilter(
                        7UL,
                        11UL),
                    Is.True);

                Assert.That(
                    workspace.SessionFilter,
                    Is.Not.Null);

                Assert.That(
                    workspace.VisibleEvidence.Count,
                    Is.EqualTo(4));

                Assert.That(
                    workspace.TrySetSessionFilter(
                        "7",
                        "11"),
                    Is.True);

                Assert.That(
                    workspace.TrySetSessionFilter(
                        7UL,
                        99UL),
                    Is.False);

                Assert.That(
                    workspace.SessionFilter
                        .SessionIdValue,
                    Is.EqualTo(11UL));

                Assert.That(
                    workspace.TrySetSessionFilter(
                        "invalid",
                        "11"),
                    Is.False);

                workspace.ClearSessionFilter();

                Assert.That(
                    workspace.SessionFilter,
                    Is.Null);
            }
        }

        [Test]
        public void PlaybackAndWorkspaceSelectionStaySynchronized()
        {
            using (var workspace =
                BuildWorkspace().Workspace)
            {
                var selectionEvents = 0;

                workspace.SelectionChanged +=
                    () => selectionEvents++;

                Assert.That(
                    workspace.SelectEvidenceByOrdinal(2),
                    Is.True);

                Assert.That(
                    workspace.Playback.CurrentIndex,
                    Is.EqualTo(2));

                Assert.That(
                    workspace.SelectedEvidence.Ordinal,
                    Is.EqualTo(2));

                Assert.That(
                    workspace.SelectedSession.ClientIdValue,
                    Is.EqualTo(7UL));

                Assert.That(
                    workspace.Playback.StepForward(),
                    Is.True);

                Assert.That(
                    workspace.SelectedEvidence.Ordinal,
                    Is.EqualTo(3));

                Assert.That(
                    selectionEvents,
                    Is.EqualTo(2));
            }
        }

        [Test]
        public void VisibleSelectionMapsToTimelineIndex()
        {
            using (var workspace =
                BuildWorkspace().Workspace)
            {
                workspace.SetOutcomeFilter(
                    InvestigationOutcomeFilter
                        .Rejected);

                Assert.That(
                    workspace.SelectedVisibleIndex,
                    Is.EqualTo(-1));

                Assert.That(
                    workspace.IsSelectedEvidenceVisible,
                    Is.False);

                Assert.That(
                    workspace.SelectVisibleEvidence(1),
                    Is.True);

                Assert.That(
                    workspace.Playback.CurrentIndex,
                    Is.EqualTo(2));

                Assert.That(
                    workspace.SelectedEvidence.Ordinal,
                    Is.EqualTo(2));

                Assert.That(
                    workspace.SelectedVisibleIndex,
                    Is.EqualTo(1));

                Assert.That(
                    workspace.IsSelectedEvidenceVisible,
                    Is.True);

                Assert.That(
                    workspace.SelectVisibleEvidence(2),
                    Is.False);
            }
        }

        [Test]
        public void InvalidBundleDoesNotCreateWorkspace()
        {
            var load =
                InvestigationBundleLoader.LoadFromFile(
                    FixturePath());

            load.Bundle.evidence[1].previousDigest =
                new string('d', 64);

            var result =
                InvestigationWorkspaceBuilder.Build(
                    load.Bundle);

            Assert.That(result.IsValid, Is.False);
            Assert.That(result.Workspace, Is.Null);
            Assert.That(
                result.Validation.IsValid,
                Is.False);
        }

        private static InvestigationWorkspaceBuildResult
            BuildWorkspace()
        {
            var load =
                InvestigationBundleLoader.LoadFromFile(
                    FixturePath());

            Assert.That(
                load.Validation.IsValid,
                Is.True);

            var result =
                InvestigationWorkspaceBuilder.Build(
                    load.Bundle);

            Assert.That(
                result.IsValid,
                Is.True);

            return result;
        }

        private static string FixturePath()
        {
            return Path.Combine(
                Application.dataPath,
                "Tickline",
                "Forensics",
                "Tests",
                "EditMode",
                "Fixtures",
                "valid-investigation-bundle.json");
        }
    }
}
