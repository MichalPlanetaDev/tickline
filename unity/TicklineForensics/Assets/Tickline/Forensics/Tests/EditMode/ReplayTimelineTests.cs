using System.IO;
using System.Linq;
using NUnit.Framework;
using UnityEngine;

namespace Tickline.Forensics.Tests
{
    public sealed class ReplayTimelineTests
    {
        [Test]
        public void NativeBundleBuildsDeterministicTimeline()
        {
            var result = BuildFixtureTimeline();

            Assert.That(
                result.IsValid,
                Is.True,
                FormatIssues(result.Validation));

            Assert.That(result.Timeline.Count, Is.EqualTo(4));

            Assert.That(
                result.Timeline.Entries.Select(
                    entry => entry.Ordinal),
                Is.EqualTo(new[] { 0, 1, 2, 3 }));

            Assert.That(
                result.Timeline[0].IsAccepted,
                Is.True);

            Assert.That(
                result.Timeline[1].IsRejected,
                Is.True);
        }

        [Test]
        public void TimelinePreservesEvidenceChain()
        {
            var timeline = BuildFixtureTimeline().Timeline;

            Assert.That(
                timeline[0].PreviousDigest,
                Is.EqualTo(timeline.InitialHeadDigest));

            for (var index = 1; index < timeline.Count; index++)
            {
                Assert.That(
                    timeline[index].PreviousDigest,
                    Is.EqualTo(
                        timeline[index - 1].RecordDigest));
            }

            Assert.That(
                timeline[timeline.Count - 1].RecordDigest,
                Is.EqualTo(timeline.TrustedHeadDigest));
        }

        [Test]
        public void NearestTickSelectionIsDeterministic()
        {
            var timeline = BuildFixtureTimeline().Timeline;

            Assert.That(
                timeline.FindNearestIndexToTick(0),
                Is.EqualTo(0));

            Assert.That(
                timeline.FindNearestIndexToTick(2),
                Is.EqualTo(0));

            Assert.That(
                timeline.FindNearestIndexToTick(3),
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
                        issue.Code ==
                        "evidence_chain_broken"),
                Is.True);
        }

        private static ReplayTimelineBuildResult
            BuildFixtureTimeline()
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
