using System;
using System.IO;
using NUnit.Framework;
using UnityEngine;

namespace Tickline.Forensics.Tests
{
    public sealed class InvestigationViewerSessionTests
    {
        [Test]
        public void ValidBundleCreatesLoadedSession()
        {
            using (var session =
                new InvestigationViewerSession())
            {
                var loaded =
                    session.TryLoadFromFile(
                        FixturePath());

                Assert.That(loaded, Is.True);

                Assert.That(
                    session.State,
                    Is.EqualTo(
                        InvestigationViewerState.Loaded));

                Assert.That(session.Bundle, Is.Not.Null);
                Assert.That(session.Workspace, Is.Not.Null);

                Assert.That(
                    session.Workspace.Evidence.Count,
                    Is.EqualTo(4));

                Assert.That(
                    session.Workspace.SelectedEvidence.Ordinal,
                    Is.EqualTo(0));

                Assert.That(
                    session.LastLoad.Succeeded,
                    Is.True);

                Assert.That(
                    session.Source,
                    Is.EqualTo(FixturePath()));

                Assert.That(
                    session.LastLoad.ErrorSummary,
                    Is.Empty);
            }
        }

        [Test]
        public void FailedLoadPreservesActiveWorkspace()
        {
            using (var session =
                new InvestigationViewerSession())
            {
                Assert.That(
                    session.TryLoadFromFile(
                        FixturePath()),
                    Is.True);

                var originalBundle = session.Bundle;
                var originalWorkspace =
                    session.Workspace;

                Assert.That(
                    originalWorkspace
                        .SelectEvidenceByOrdinal(2),
                    Is.True);

                var loaded =
                    session.TryLoadFromJson(
                        string.Empty,
                        "invalid-memory-input");

                Assert.That(loaded, Is.False);

                Assert.That(
                    session.Bundle,
                    Is.SameAs(originalBundle));

                Assert.That(
                    session.Workspace,
                    Is.SameAs(originalWorkspace));

                Assert.That(
                    session.Workspace
                        .SelectedEvidence.Ordinal,
                    Is.EqualTo(2));

                Assert.That(
                    session.State,
                    Is.EqualTo(
                        InvestigationViewerState.Loaded));

                Assert.That(
                    session.Source,
                    Is.EqualTo(FixturePath()));

                Assert.That(
                    session.LastLoad.Succeeded,
                    Is.False);

                Assert.That(
                    session.LastLoad.Source,
                    Is.EqualTo(
                        "invalid-memory-input"));

                Assert.That(
                    session.LastLoad.ErrorSummary,
                    Does.Contain("bundle_json_empty"));
            }
        }

        [Test]
        public void SuccessfulReplacementCreatesFreshWorkspace()
        {
            using (var session =
                new InvestigationViewerSession())
            {
                Assert.That(
                    session.TryLoadFromFile(
                        FixturePath()),
                    Is.True);

                var firstWorkspace =
                    session.Workspace;

                Assert.That(
                    firstWorkspace
                        .SelectEvidenceByOrdinal(3),
                    Is.True);

                Assert.That(
                    session.TryLoadFromFile(
                        FixturePath()),
                    Is.True);

                Assert.That(
                    session.Workspace,
                    Is.Not.SameAs(firstWorkspace));

                Assert.That(
                    session.Workspace
                        .SelectedEvidence.Ordinal,
                    Is.EqualTo(0));

                Assert.That(
                    session.Workspace
                        .Playback.CurrentIndex,
                    Is.EqualTo(0));
            }
        }

        [Test]
        public void LoadAndStateEventsAreDeterministic()
        {
            using (var session =
                new InvestigationViewerSession())
            {
                var loadEvents = 0;
                var stateEvents = 0;
                InvestigationLoadResult observed = null;

                session.LoadAttempted += result =>
                {
                    loadEvents++;
                    observed = result;
                };

                session.StateChanged +=
                    () => stateEvents++;

                Assert.That(
                    session.TryLoadFromJson(
                        string.Empty),
                    Is.False);

                Assert.That(loadEvents, Is.EqualTo(1));
                Assert.That(stateEvents, Is.EqualTo(0));
                Assert.That(observed.Succeeded, Is.False);

                Assert.That(
                    session.TryLoadFromFile(
                        FixturePath()),
                    Is.True);

                Assert.That(loadEvents, Is.EqualTo(2));
                Assert.That(stateEvents, Is.EqualTo(1));
                Assert.That(observed.Succeeded, Is.True);

                session.Clear();

                Assert.That(stateEvents, Is.EqualTo(2));

                Assert.That(
                    session.State,
                    Is.EqualTo(
                        InvestigationViewerState.Empty));
            }
        }

        [Test]
        public void ClearRemovesActiveInvestigation()
        {
            using (var session =
                new InvestigationViewerSession())
            {
                Assert.That(
                    session.TryLoadFromFile(
                        FixturePath()),
                    Is.True);

                session.Clear();

                Assert.That(
                    session.State,
                    Is.EqualTo(
                        InvestigationViewerState.Empty));

                Assert.That(session.Bundle, Is.Null);
                Assert.That(session.Workspace, Is.Null);
                Assert.That(session.Source, Is.Empty);

                session.Clear();

                Assert.That(
                    session.State,
                    Is.EqualTo(
                        InvestigationViewerState.Empty));
            }
        }

        [Test]
        public void DisposedSessionRejectsOperations()
        {
            var session =
                new InvestigationViewerSession();

            session.Dispose();

            Assert.Throws<ObjectDisposedException>(
                () => session.TryLoadFromFile(
                    FixturePath()));

            Assert.Throws<ObjectDisposedException>(
                () => session.TryLoadFromJson(
                    "{}"));

            Assert.Throws<ObjectDisposedException>(
                () => session.Clear());

            session.Dispose();
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
