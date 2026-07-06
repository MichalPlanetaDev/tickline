using System;
using System.IO;
using NUnit.Framework;
using Tickline.Forensics.Editor;
using UnityEngine;

namespace Tickline.Forensics.Tests
{
    public sealed class InvestigationViewerControllerTests
    {
        [Test]
        public void ValidLoadExposesWorkspaceAndSource()
        {
            using (var controller =
                new InvestigationViewerController())
            {
                var changeCount = 0;

                controller.Changed +=
                    () => changeCount++;

                Assert.That(
                    controller.LoadFromFile(FixturePath()),
                    Is.True);

                Assert.That(controller.IsLoaded, Is.True);
                Assert.That(controller.Bundle, Is.Not.Null);
                Assert.That(controller.Workspace, Is.Not.Null);

                Assert.That(
                    controller.Source,
                    Is.EqualTo(FixturePath()));

                Assert.That(
                    controller.Workspace.Evidence.Count,
                    Is.EqualTo(4));

                Assert.That(changeCount, Is.EqualTo(1));
            }
        }

        [Test]
        public void FailedReplacementPreservesActiveView()
        {
            using (var controller =
                new InvestigationViewerController())
            {
                Assert.That(
                    controller.LoadFromFile(FixturePath()),
                    Is.True);

                var originalWorkspace =
                    controller.Workspace;

                Assert.That(
                    controller.SelectOrdinal(2),
                    Is.True);

                Assert.That(
                    controller.Session.TryLoadFromJson(
                        string.Empty,
                        "invalid-replacement"),
                    Is.False);

                Assert.That(
                    controller.Workspace,
                    Is.SameAs(originalWorkspace));

                Assert.That(
                    controller.Workspace
                        .SelectedEvidence.Ordinal,
                    Is.EqualTo(2));

                Assert.That(
                    controller.Source,
                    Is.EqualTo(FixturePath()));

                Assert.That(
                    controller.LastLoad.Succeeded,
                    Is.False);
            }
        }

        [Test]
        public void FiltersDelegateToWorkspace()
        {
            using (var controller =
                LoadedController())
            {
                Assert.That(
                    controller.SetOutcomeFilter(
                        InvestigationOutcomeFilter.Rejected),
                    Is.True);

                Assert.That(
                    controller.Workspace
                        .VisibleEvidence.Count,
                    Is.EqualTo(2));

                Assert.That(
                    controller.SetSessionFilterIndex(1),
                    Is.True);

                Assert.That(
                    controller.SessionFilterIndex,
                    Is.EqualTo(1));

                Assert.That(
                    controller.Workspace.SessionFilter,
                    Is.Not.Null);

                Assert.That(
                    controller.SetSessionFilterIndex(0),
                    Is.True);

                Assert.That(
                    controller.SessionFilterIndex,
                    Is.EqualTo(0));

                Assert.That(
                    controller.SetSessionFilterIndex(99),
                    Is.False);
            }
        }

        [Test]
        public void VisibleSelectionUsesOriginalTimelineIndex()
        {
            using (var controller =
                LoadedController())
            {
                controller.SetOutcomeFilter(
                    InvestigationOutcomeFilter.Rejected);

                Assert.That(
                    controller.SelectVisibleEvidence(1),
                    Is.True);

                Assert.That(
                    controller.Workspace
                        .Playback.CurrentIndex,
                    Is.EqualTo(2));

                Assert.That(
                    controller.Workspace
                        .SelectedEvidence.Ordinal,
                    Is.EqualTo(2));
            }
        }

        [Test]
        public void PlaybackControlsRemainSynchronized()
        {
            using (var controller =
                LoadedController())
            {
                controller.SetPlaybackSpeed(2.0);

                Assert.That(
                    controller.Workspace
                        .Playback.PlaybackSpeed,
                    Is.EqualTo(2.0));

                Assert.That(controller.Play(), Is.True);

                Assert.That(
                    controller.Advance(0.25),
                    Is.EqualTo(1));

                Assert.That(
                    controller.Workspace
                        .Playback.CurrentIndex,
                    Is.EqualTo(1));

                Assert.That(
                    controller.StepForward(),
                    Is.True);

                Assert.That(
                    controller.Workspace
                        .SelectedEvidence.Ordinal,
                    Is.EqualTo(2));

                controller.ResetPlayback();

                Assert.That(
                    controller.Workspace
                        .Playback.CurrentIndex,
                    Is.EqualTo(0));
            }
        }

        [Test]
        public void ClearReturnsControllerToEmptyState()
        {
            using (var controller =
                LoadedController())
            {
                controller.Clear();

                Assert.That(controller.IsLoaded, Is.False);
                Assert.That(controller.Bundle, Is.Null);
                Assert.That(controller.Workspace, Is.Null);
                Assert.That(controller.Source, Is.Empty);
            }
        }

        [Test]
        public void DisposedControllerRejectsOperations()
        {
            var controller =
                new InvestigationViewerController();

            controller.Dispose();

            Assert.Throws<ObjectDisposedException>(
                () => controller.LoadFromFile(
                    FixturePath()));

            Assert.Throws<ObjectDisposedException>(
                () => controller.Clear());

            controller.Dispose();
        }

        private static InvestigationViewerController
            LoadedController()
        {
            var controller =
                new InvestigationViewerController();

            if (!controller.LoadFromFile(FixturePath()))
            {
                controller.Dispose();

                throw new InvalidOperationException(
                    "fixture could not be loaded");
            }

            return controller;
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
