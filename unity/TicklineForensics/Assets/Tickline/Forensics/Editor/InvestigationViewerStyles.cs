using UnityEditor;
using UnityEngine;

namespace Tickline.Forensics.Editor
{
    internal sealed class InvestigationViewerStyles
    {
        private InvestigationViewerStyles()
        {
            Title = new GUIStyle(EditorStyles.largeLabel)
            {
                fontSize = 18,
                fontStyle = FontStyle.Bold,
                margin = new RectOffset(4, 4, 6, 8)
            };

            Subtitle = new GUIStyle(EditorStyles.wordWrappedMiniLabel)
            {
                margin = new RectOffset(4, 4, 0, 8)
            };

            SectionHeader = new GUIStyle(EditorStyles.boldLabel)
            {
                margin = new RectOffset(2, 2, 6, 4)
            };

            SummaryValue = new GUIStyle(EditorStyles.largeLabel)
            {
                fontSize = 15,
                fontStyle = FontStyle.Bold,
                alignment = TextAnchor.MiddleLeft
            };

            SummaryDetail = new GUIStyle(EditorStyles.wordWrappedMiniLabel)
            {
                alignment = TextAnchor.UpperLeft
            };

            EvidenceRow = new GUIStyle(EditorStyles.miniButton)
            {
                alignment = TextAnchor.MiddleLeft,
                fixedHeight = 24,
                padding = new RectOffset(8, 8, 3, 3)
            };

            EmptyState = new GUIStyle(EditorStyles.wordWrappedLabel)
            {
                alignment = TextAnchor.MiddleCenter,
                fontSize = 13
            };

            Digest = new GUIStyle(EditorStyles.textField)
            {
                alignment = TextAnchor.MiddleLeft,
                wordWrap = false
            };

            CountLabel = new GUIStyle(EditorStyles.miniLabel)
            {
                alignment = TextAnchor.MiddleRight
            };
        }

        public GUIStyle Title { get; }

        public GUIStyle Subtitle { get; }

        public GUIStyle SectionHeader { get; }

        public GUIStyle SummaryValue { get; }

        public GUIStyle SummaryDetail { get; }

        public GUIStyle EvidenceRow { get; }

        public GUIStyle EmptyState { get; }

        public GUIStyle Digest { get; }

        public GUIStyle CountLabel { get; }

        public static InvestigationViewerStyles Create()
        {
            return new InvestigationViewerStyles();
        }
    }
}
