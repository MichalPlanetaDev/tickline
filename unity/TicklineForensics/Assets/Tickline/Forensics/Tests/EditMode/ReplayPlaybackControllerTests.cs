using System;
using System.IO;
using NUnit.Framework;
using UnityEngine;

namespace Tickline.Forensics.Tests
{
    public sealed class ReplayPlaybackControllerTests
    {
        [Test]
        public void ControllerStartsAtFirstEntry()
        {
            var controller = CreateController();

            Assert.That(controller.CurrentIndex, Is.EqualTo(0));
            Assert.That(controller.CurrentEntry, Is.Not.Null);
            Assert.That(
                controller.CurrentEntry.Ordinal,
                Is.EqualTo(0));
            Assert.That(controller.IsPlaying, Is.False);
        }

        [Test]
        public void ManualStepsRespectTimelineBounds()
        {
            var controller = CreateController();

            Assert.That(controller.StepBackward(), Is.False);
            Assert.That(controller.CurrentIndex, Is.EqualTo(0));

            Assert.That(controller.StepForward(), Is.True);
            Assert.That(controller.CurrentIndex, Is.EqualTo(1));

            Assert.That(controller.StepForward(), Is.False);
            Assert.That(controller.CurrentIndex, Is.EqualTo(1));

            Assert.That(controller.StepBackward(), Is.True);
            Assert.That(controller.CurrentIndex, Is.EqualTo(0));
        }

        [Test]
        public void PlaybackAdvancesAfterConfiguredInterval()
        {
            var controller = CreateController(1.0);

            Assert.That(controller.Play(), Is.True);
            Assert.That(controller.Advance(0.4), Is.EqualTo(0));
            Assert.That(controller.CurrentIndex, Is.EqualTo(0));
            Assert.That(controller.IsPlaying, Is.True);

            Assert.That(controller.Advance(0.6), Is.EqualTo(1));
            Assert.That(controller.CurrentIndex, Is.EqualTo(1));
            Assert.That(controller.IsPlaying, Is.False);
        }

        [Test]
        public void PlaybackSpeedScalesElapsedTime()
        {
            var controller = CreateController(1.0);

            controller.SetPlaybackSpeed(2.0);

            Assert.That(controller.Play(), Is.True);
            Assert.That(controller.Advance(0.5), Is.EqualTo(1));
            Assert.That(controller.CurrentIndex, Is.EqualTo(1));
        }

        [Test]
        public void SeekAndResetPublishPositionChanges()
        {
            var controller = CreateController();
            var notifiedIndex = -1;

            controller.PositionChanged +=
                index => notifiedIndex = index;

            Assert.That(controller.SeekToTick(11), Is.True);
            Assert.That(controller.CurrentIndex, Is.EqualTo(1));
            Assert.That(notifiedIndex, Is.EqualTo(1));

            controller.Reset();

            Assert.That(controller.CurrentIndex, Is.EqualTo(0));
            Assert.That(notifiedIndex, Is.EqualTo(0));
        }

        [Test]
        public void InvalidPlaybackValuesAreRejected()
        {
            var controller = CreateController();

            Assert.Throws<ArgumentOutOfRangeException>(
                () => controller.SetPlaybackSpeed(0.0));

            Assert.Throws<ArgumentOutOfRangeException>(
                () => controller.SetPlaybackSpeed(double.NaN));

            Assert.Throws<ArgumentOutOfRangeException>(
                () => controller.Advance(-0.1));
        }

        private static ReplayPlaybackController CreateController(
            double secondsPerEntry = 0.5)
        {
            var path = Path.Combine(
                Application.dataPath,
                "Tickline",
                "Forensics",
                "Tests",
                "EditMode",
                "Fixtures",
                "valid-investigation-bundle.json");

            var load =
                InvestigationBundleLoader.LoadFromFile(path);

            Assert.That(load.Validation.IsValid, Is.True);

            var timeline =
                ReplayTimelineBuilder.Build(load.Bundle);

            Assert.That(timeline.IsValid, Is.True);

            return new ReplayPlaybackController(
                timeline.Timeline,
                secondsPerEntry);
        }
    }
}
