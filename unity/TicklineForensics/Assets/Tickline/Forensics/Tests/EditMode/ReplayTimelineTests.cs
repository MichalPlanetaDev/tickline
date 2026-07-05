using System.IO;
using System.Linq;
using NUnit.Framework;
using UnityEngine;

namespace Tickline.Forensics.Tests
{
    public sealed class ReplayTimelineTests
    {
        [Test]
        public void ValidBundleBuildsDeterministicTimeline()
        {
            var result = BuildFixtureTimeline();

            Assert.That(
                result.IsValid,
                Is.True,
                FormatIssues(result.Validation));

            Assert.That(result.Timeline.Count, Is.EqualTo(2));

            var first = result.Timeline[0];
            var second = result.Timeline[1];

            Assert.That(first.Ordinal, Is.EqualTo(0));
            Assert.That(first.TargetTick, Is.EqualTo(10UL));
            Assert.That(first.SessionSequence, Is.EqualTo(1UL));
            Assert.That(first.IsAccepted, Is.True);

            Assert.That(second.Ordinal, Is.EqualTo(1));
            Assert.That(second.TargetTick, Is.EqualTo(11UL));
            Assert.That(second.SessionSequence, Is.EqualTo(2UL));
            Assert.That(second.IsRejected, Is.True);
            Assert.That(
                second.RejectionCode,
                Is.EqualTo("sequence_replayed"));
        }

        [Test]
        public void TimelinePreservesEvidenceOrder()
        {
            var result = BuildFixtureTimeline();

            Assert.That(
                result.Timeline.Entries.Select(
                    entry => entry.Ordinal),
                Is.EqualTo(new[] { 0, 1 }));

            Assert.That(
                result.Timeline.Entries.Select(
                    entry => entry.RecordDigest),
                Is.EqualTo(
                    new[]
                    {
                        new string('b', 64),
                        new string('c', 64)
                    }));
        }

        [Test]
        public void NearestTickSelectionIsDeterministic()
        {
            var timeline = BuildFixtureTimeline().Timeline;

            Assert.That(
                timeline.FindNearestIndexToTick(0),
                Is.EqualTo(0));

            Assert.That(
                timeline.FindNearestIndexToTick(10),
                Is.EqualTo(0));

            Assert.That(
                timeline.FindNearestIndexToTick(11),
                Is.EqualTo(1));

            Assert.That(
                timeline.FindNearestIndexToTick(100),
                Is.EqualTo(1));
        }

        [Test]
        public void InvalidBundleDoesNotProduceTimeline()
        {
            var load =
                InvestigationBundleLoader.LoadFromFile(
                    FixturePath());

            load.Bundle.evidence[1].previousDigest =
                new string('d', 64);

            var result =
                ReplayTimelineBuilder.Build(load.Bundle);

            Assert.That(result.IsValid, Is.False);
            Assert.That(result.Timeline, Is.Null);
            Assert.That(
                result.Validation.Issues.Any(
                    issue =>
                        issue.Code == "evidence_chain_broken"),
                Is.True);
        }

        private static ReplayTimelineBuildResult BuildFixtureTimeline()
        {
            var load =
                InvestigationBundleLoader.LoadFromFile(
                    FixturePath());

            Assert.That(
                load.Validation.IsValid,
                Is.True,
                FormatIssues(load.Validation));

            return ReplayTimelineBuilder.Build(load.Bundle);
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

        private static string FormatIssues(
            BundleValidationResult validation)
        {
            return string.Join(
                "\n",
                validation.Issues.Select(
                    issue =>
                        $"{issue.Code} at {issue.Path}: " +
                        issue.Message));
        }
    }
}
