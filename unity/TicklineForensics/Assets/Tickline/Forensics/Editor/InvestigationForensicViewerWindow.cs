using System;
using System.Globalization;
using System.IO;
using UnityEditor;
using UnityEngine;

namespace Tickline.Forensics.Editor
{
    public sealed class InvestigationForensicViewerWindow :
        EditorWindow
    {
        private static readonly string[] PlaybackSpeedLabels =
        {
            "0.25x",
            "0.5x",
            "1x",
            "2x",
            "4x"
        };

        private static readonly double[] PlaybackSpeeds =
        {
            0.25,
            0.5,
            1.0,
            2.0,
            4.0
        };

        private InvestigationViewerController controller;
        private InvestigationViewerStyles styles;

        private Vector2 evidenceScroll;
        private Vector2 detailScroll;
        private double lastUpdateTime;

        [MenuItem("Window/Tickline/Forensic Replay Viewer")]
        public static void OpenWindow()
        {
            var window =
                GetWindow<InvestigationForensicViewerWindow>();

            window.titleContent =
                new GUIContent("Tickline Forensics");

            window.minSize = new Vector2(920f, 620f);
            window.Show();
        }

        private void OnEnable()
        {
            titleContent =
                new GUIContent("Tickline Forensics");

            minSize = new Vector2(920f, 620f);

            EnsureController();

            lastUpdateTime =
                EditorApplication.timeSinceStartup;

            EditorApplication.update +=
                HandleEditorUpdate;
        }

        private void OnDisable()
        {
            EditorApplication.update -=
                HandleEditorUpdate;

            if (controller != null)
            {
                controller.Changed -=
                    HandleControllerChanged;

                controller.Dispose();
                controller = null;
            }
        }

        private void OnGUI()
        {
            EnsureController();

            if (styles == null)
            {
                styles =
                    InvestigationViewerStyles.Create();
            }

            DrawToolbar();
            DrawLoadFailure();

            if (!controller.IsLoaded)
            {
                DrawEmptyState();
                return;
            }

            DrawLoadedState();
        }

        private void EnsureController()
        {
            if (controller != null)
            {
                return;
            }

            controller =
                new InvestigationViewerController();

            controller.Changed +=
                HandleControllerChanged;
        }

        private void HandleControllerChanged()
        {
            Repaint();
        }

        private void HandleEditorUpdate()
        {
            var now =
                EditorApplication.timeSinceStartup;

            var elapsed = now - lastUpdateTime;
            lastUpdateTime = now;

            if (controller == null ||
                controller.Workspace == null ||
                !controller.Workspace.Playback.IsPlaying)
            {
                return;
            }

            controller.Advance(
                Math.Max(0.0, elapsed));

            Repaint();
        }

        private void DrawToolbar()
        {
            EditorGUILayout.BeginHorizontal(
                EditorStyles.toolbar);

            if (GUILayout.Button(
                    "Open Bundle",
                    EditorStyles.toolbarButton,
                    GUILayout.Width(92f)))
            {
                OpenBundle();
            }

            using (new EditorGUI.DisabledScope(
                !controller.IsLoaded))
            {
                if (GUILayout.Button(
                        "Clear",
                        EditorStyles.toolbarButton,
                        GUILayout.Width(58f)))
                {
                    controller.Clear();
                }
            }

            GUILayout.Space(8f);

            if (controller.IsLoaded)
            {
                GUILayout.Label(
                    Path.GetFileName(controller.Source),
                    EditorStyles.miniLabel);

                GUILayout.FlexibleSpace();

                GUILayout.Label(
                    controller.Source,
                    EditorStyles.miniLabel,
                    GUILayout.MaxWidth(
                        Math.Max(
                            240f,
                            position.width * 0.42f)));
            }
            else
            {
                GUILayout.FlexibleSpace();
                GUILayout.Label(
                    "No investigation loaded",
                    EditorStyles.miniLabel);
            }

            EditorGUILayout.EndHorizontal();
        }

        private void OpenBundle()
        {
            var initialDirectory =
                ResolveInitialDirectory();

            var path =
                EditorUtility.OpenFilePanel(
                    "Open Tickline Investigation Bundle",
                    initialDirectory,
                    "json");

            if (string.IsNullOrEmpty(path))
            {
                return;
            }

            controller.LoadFromFile(path);

            lastUpdateTime =
                EditorApplication.timeSinceStartup;
        }

        private string ResolveInitialDirectory()
        {
            if (!string.IsNullOrEmpty(controller.Source))
            {
                var directory =
                    Path.GetDirectoryName(
                        controller.Source);

                if (!string.IsNullOrEmpty(directory) &&
                    Directory.Exists(directory))
                {
                    return directory;
                }
            }

            return Application.dataPath;
        }

        private void DrawLoadFailure()
        {
            var lastLoad = controller.LastLoad;

            if (lastLoad == null ||
                lastLoad.Succeeded ||
                string.IsNullOrEmpty(
                    lastLoad.ErrorSummary))
            {
                return;
            }

            EditorGUILayout.HelpBox(
                "The selected bundle was rejected. " +
                "The active investigation was preserved.\n\n" +
                lastLoad.ErrorSummary,
                MessageType.Error);
        }

        private void DrawEmptyState()
        {
            GUILayout.FlexibleSpace();

            EditorGUILayout.BeginHorizontal();
            GUILayout.FlexibleSpace();

            EditorGUILayout.BeginVertical(
                EditorStyles.helpBox,
                GUILayout.Width(520f));

            GUILayout.Label(
                "Tickline Forensic Replay Viewer",
                styles.Title);

            GUILayout.Label(
                "Open a native investigation-bundle JSON file " +
                "to inspect deterministic command evidence, " +
                "session state, integrity findings, and replay " +
                "progress.",
                styles.EmptyState);

            GUILayout.Space(12f);

            if (GUILayout.Button(
                    "Open Investigation Bundle",
                    GUILayout.Height(34f)))
            {
                OpenBundle();
            }

            EditorGUILayout.EndVertical();

            GUILayout.FlexibleSpace();
            EditorGUILayout.EndHorizontal();

            GUILayout.FlexibleSpace();
        }

        private void DrawLoadedState()
        {
            GUILayout.Label(
                "Investigation Replay",
                styles.Title);

            GUILayout.Label(
                "Immutable evidence inspection with deterministic " +
                "selection, filtering, and replay controls.",
                styles.Subtitle);

            DrawSummary();
            DrawFilters();
            DrawPlaybackControls();

            GUILayout.Space(4f);

            EditorGUILayout.BeginHorizontal();

            DrawEvidencePanel();
            DrawDetailPanel();

            EditorGUILayout.EndHorizontal();
        }

        private void DrawSummary()
        {
            var workspace = controller.Workspace;
            var bundle = controller.Bundle;
            var integrity = workspace.Integrity;

            EditorGUILayout.BeginHorizontal();

            DrawSummaryCard(
                "Integrity",
                integrity.StatusCode,
                integrity.EvidenceChainContinuous &&
                integrity.TrustedHeadMatches
                    ? "Evidence chain and trusted head match."
                    : "Integrity findings require review.");

            DrawSummaryCard(
                "Replay",
                integrity.ReplayVerified
                    ? "verified"
                    : "not verified",
                "Final tick " +
                bundle.replay.finalTick +
                " · fingerprint " +
                bundle.replay.finalWorldFingerprint);

            DrawSummaryCard(
                "Evidence",
                workspace.Evidence.Count.ToString(
                    CultureInfo.InvariantCulture),
                bundle.replay.acceptedCommands +
                " accepted · " +
                bundle.replay.rejectedCommands +
                " rejected");

            EditorGUILayout.EndHorizontal();

            var messageType =
                integrity.Status ==
                InvestigationIntegrityStatus.ReplayVerified
                    ? MessageType.Info
                    : MessageType.Warning;

            EditorGUILayout.HelpBox(
                integrity.Status ==
                InvestigationIntegrityStatus.ReplayVerified
                    ? "The bundle passed structural validation, " +
                      "the evidence chain reaches the trusted head, " +
                      "and native replay metadata reports verification."
                    : "The bundle is loaded, but one or more replay " +
                      "integrity conditions are not verified.",
                messageType);
        }

        private void DrawSummaryCard(
            string title,
            string value,
            string detail)
        {
            EditorGUILayout.BeginVertical(
                EditorStyles.helpBox,
                GUILayout.MinWidth(220f),
                GUILayout.ExpandWidth(true));

            GUILayout.Label(
                title,
                styles.SectionHeader);

            GUILayout.Label(
                value,
                styles.SummaryValue);

            GUILayout.Label(
                detail,
                styles.SummaryDetail);

            EditorGUILayout.EndVertical();
        }

        private void DrawFilters()
        {
            var workspace = controller.Workspace;

            EditorGUILayout.BeginHorizontal(
                EditorStyles.toolbar);

            GUILayout.Label(
                "Outcome",
                GUILayout.Width(56f));

            var selectedOutcome =
                (InvestigationOutcomeFilter)
                EditorGUILayout.EnumPopup(
                    workspace.OutcomeFilter,
                    EditorStyles.toolbarPopup,
                    GUILayout.Width(108f));

            if (selectedOutcome !=
                workspace.OutcomeFilter)
            {
                controller.SetOutcomeFilter(
                    selectedOutcome);
            }

            GUILayout.Space(10f);

            GUILayout.Label(
                "Session",
                GUILayout.Width(52f));

            var sessionOptions =
                BuildSessionOptions();

            var currentSessionIndex =
                controller.SessionFilterIndex;

            var selectedSessionIndex =
                EditorGUILayout.Popup(
                    currentSessionIndex,
                    sessionOptions,
                    EditorStyles.toolbarPopup,
                    GUILayout.Width(190f));

            if (selectedSessionIndex !=
                currentSessionIndex)
            {
                controller.SetSessionFilterIndex(
                    selectedSessionIndex);
            }

            GUILayout.FlexibleSpace();

            GUILayout.Label(
                workspace.VisibleEvidence.Count +
                " of " +
                workspace.Evidence.Count +
                " records",
                styles.CountLabel);

            EditorGUILayout.EndHorizontal();
        }

        private string[] BuildSessionOptions()
        {
            var sessions =
                controller.Workspace.Sessions;

            var options =
                new string[sessions.Count + 1];

            options[0] = "All sessions";

            for (var index = 0;
                 index < sessions.Count;
                 index++)
            {
                var session = sessions[index];

                options[index + 1] =
                    session.ClientId +
                    " / " +
                    session.SessionId;
            }

            return options;
        }

        private void DrawPlaybackControls()
        {
            var playback =
                controller.Workspace.Playback;

            EditorGUILayout.BeginHorizontal(
                EditorStyles.helpBox);

            using (new EditorGUI.DisabledScope(
                playback.Timeline.Count == 0))
            {
                if (GUILayout.Button(
                        "First",
                        GUILayout.Width(54f)))
                {
                    controller.ResetPlayback();
                }

                if (GUILayout.Button(
                        "Back",
                        GUILayout.Width(54f)))
                {
                    controller.StepBackward();
                }

                if (GUILayout.Button(
                        playback.IsPlaying
                            ? "Pause"
                            : "Play",
                        GUILayout.Width(62f)))
                {
                    controller.TogglePlayback();

                    lastUpdateTime =
                        EditorApplication.timeSinceStartup;
                }

                if (GUILayout.Button(
                        "Forward",
                        GUILayout.Width(64f)))
                {
                    controller.StepForward();
                }
            }

            GUILayout.Space(8f);

            GUILayout.Label(
                "Speed",
                GUILayout.Width(42f));

            var currentSpeedIndex =
                FindPlaybackSpeedIndex(
                    playback.PlaybackSpeed);

            var selectedSpeedIndex =
                EditorGUILayout.Popup(
                    currentSpeedIndex,
                    PlaybackSpeedLabels,
                    GUILayout.Width(68f));

            if (selectedSpeedIndex !=
                currentSpeedIndex)
            {
                controller.SetPlaybackSpeed(
                    PlaybackSpeeds[
                        selectedSpeedIndex]);
            }

            GUILayout.Space(8f);

            if (playback.Timeline.Count > 0)
            {
                var selectedIndex =
                    EditorGUILayout.IntSlider(
                        playback.CurrentIndex,
                        0,
                        playback.Timeline.Count - 1);

                if (selectedIndex !=
                    playback.CurrentIndex)
                {
                    controller.SelectTimelineIndex(
                        selectedIndex);
                }

                GUILayout.Label(
                    (playback.CurrentIndex + 1) +
                    " / " +
                    playback.Timeline.Count,
                    GUILayout.Width(68f));
            }
            else
            {
                GUILayout.FlexibleSpace();
                GUILayout.Label("No evidence");
            }

            EditorGUILayout.EndHorizontal();
        }

        private static int FindPlaybackSpeedIndex(
            double speed)
        {
            var nearestIndex = 0;
            var nearestDistance =
                Math.Abs(
                    PlaybackSpeeds[0] - speed);

            for (var index = 1;
                 index < PlaybackSpeeds.Length;
                 index++)
            {
                var distance =
                    Math.Abs(
                        PlaybackSpeeds[index] -
                        speed);

                if (distance < nearestDistance)
                {
                    nearestIndex = index;
                    nearestDistance = distance;
                }
            }

            return nearestIndex;
        }

        private void DrawEvidencePanel()
        {
            var workspace = controller.Workspace;
            var panelWidth =
                Math.Max(
                    360f,
                    position.width * 0.43f);

            EditorGUILayout.BeginVertical(
                EditorStyles.helpBox,
                GUILayout.Width(panelWidth),
                GUILayout.ExpandHeight(true));

            GUILayout.Label(
                "Evidence Timeline",
                styles.SectionHeader);

            evidenceScroll =
                EditorGUILayout.BeginScrollView(
                    evidenceScroll,
                    GUILayout.ExpandHeight(true));

            if (workspace.VisibleEvidence.Count == 0)
            {
                EditorGUILayout.HelpBox(
                    "No evidence records match the active filters.",
                    MessageType.Info);
            }

            for (var index = 0;
                 index <
                 workspace.VisibleEvidence.Count;
                 index++)
            {
                var evidence =
                    workspace.VisibleEvidence[index];

                var selected =
                    evidence.TimelineIndex ==
                    workspace.Playback.CurrentIndex;

                var label =
                    BuildEvidenceLabel(evidence);

                var rowSelected =
                    GUILayout.Toggle(
                        selected,
                        label,
                        styles.EvidenceRow);

                if (rowSelected && !selected)
                {
                    controller.SelectVisibleEvidence(
                        index);
                }
            }

            EditorGUILayout.EndScrollView();
            EditorGUILayout.EndVertical();
        }

        private static string BuildEvidenceLabel(
            InvestigationEvidenceInspection evidence)
        {
            var outcome =
                evidence.IsAccepted
                    ? "ACCEPTED"
                    : "REJECTED";

            return "#" +
                evidence.Ordinal.ToString(
                    "D4",
                    CultureInfo.InvariantCulture) +
                "   tick " +
                evidence.TargetTick.ToString(
                    CultureInfo.InvariantCulture) +
                "   " +
                outcome +
                "   " +
                evidence.CommandType +
                "   " +
                evidence.ClientId +
                "/" +
                evidence.SessionId;
        }

        private void DrawDetailPanel()
        {
            EditorGUILayout.BeginVertical(
                EditorStyles.helpBox,
                GUILayout.ExpandWidth(true),
                GUILayout.ExpandHeight(true));

            GUILayout.Label(
                "Inspection",
                styles.SectionHeader);

            detailScroll =
                EditorGUILayout.BeginScrollView(
                    detailScroll,
                    GUILayout.ExpandHeight(true));

            var evidence =
                controller.Workspace.SelectedEvidence;

            if (evidence == null)
            {
                EditorGUILayout.HelpBox(
                    "Select an evidence record.",
                    MessageType.Info);

                EditorGUILayout.EndScrollView();
                EditorGUILayout.EndVertical();
                return;
            }

            DrawEvidenceDetails(evidence);
            DrawSessionDetails(evidence.Session);
            DrawChainDetails(evidence);
            DrawArchiveDetails();

            EditorGUILayout.EndScrollView();
            EditorGUILayout.EndVertical();
        }

        private void DrawEvidenceDetails(
            InvestigationEvidenceInspection evidence)
        {
            GUILayout.Label(
                "Command Evidence",
                styles.SectionHeader);

            DrawValue("Ordinal", evidence.Ordinal);
            DrawValue(
                "Timeline index",
                evidence.TimelineIndex);

            DrawValue(
                "Target tick",
                evidence.TargetTick);

            DrawValue(
                "Session sequence",
                evidence.SessionSequence);

            DrawValue(
                "Command type",
                evidence.CommandType);

            DrawValue(
                "Outcome",
                evidence.Outcome);

            DrawValue(
                "Rejection code",
                evidence.RejectionCode);

            GUILayout.Space(6f);
        }

        private void DrawSessionDetails(
            InvestigationSessionInspection session)
        {
            GUILayout.Label(
                "Session",
                styles.SectionHeader);

            DrawValue(
                "Client ID",
                session.ClientId);

            DrawValue(
                "Session ID",
                session.SessionId);

            DrawValue(
                "Last committed sequence",
                session.LastCommittedSequence);

            DrawValue(
                "Target tick range",
                session.FirstTargetTick +
                " – " +
                session.LastTargetTick);

            DrawValue(
                "Accepted commands",
                session.AcceptedCommands);

            DrawValue(
                "Rejected commands",
                session.RejectedCommands);

            GUILayout.Space(6f);
        }

        private void DrawChainDetails(
            InvestigationEvidenceInspection evidence)
        {
            GUILayout.Label(
                "Evidence Chain",
                styles.SectionHeader);

            DrawValue(
                "Previous link matches",
                evidence.PreviousDigestMatches
                    ? "yes"
                    : "no");

            DrawValue(
                "Final evidence",
                evidence.IsFinalEvidence
                    ? "yes"
                    : "no");

            DrawValue(
                "Completes trusted head",
                evidence.CompletesTrustedHead
                    ? "yes"
                    : "no");

            DrawDigest(
                "Expected previous digest",
                evidence.ExpectedPreviousDigest);

            DrawDigest(
                "Previous digest",
                evidence.PreviousDigest);

            DrawDigest(
                "Record digest",
                evidence.RecordDigest);

            GUILayout.Space(6f);
        }

        private void DrawArchiveDetails()
        {
            var bundle = controller.Bundle;

            GUILayout.Label(
                "Archive",
                styles.SectionHeader);

            DrawValue(
                "Imported at UTC",
                bundle.importedAtUtc);

            DrawValue(
                "Schema version",
                bundle.schemaVersion);

            DrawValue(
                "Final replay tick",
                bundle.replay.finalTick);

            DrawValue(
                "World fingerprint",
                bundle.replay.finalWorldFingerprint);

            DrawDigest(
                "Archive digest",
                bundle.archiveDigest);

            DrawDigest(
                "Initial head digest",
                bundle.initialHeadDigest);

            DrawDigest(
                "Trusted head digest",
                bundle.trustedHeadDigest);
        }

        private static void DrawValue(
            string label,
            object value)
        {
            EditorGUILayout.LabelField(
                label,
                Convert.ToString(
                    value,
                    CultureInfo.InvariantCulture));
        }

        private void DrawDigest(
            string label,
            string digest)
        {
            EditorGUILayout.LabelField(label);

            EditorGUILayout.SelectableLabel(
                digest ?? string.Empty,
                styles.Digest,
                GUILayout.Height(18f));
        }
    }
}
