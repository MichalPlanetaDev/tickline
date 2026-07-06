import json
import tempfile
import unittest
from dataclasses import replace
from datetime import datetime, timedelta, timezone
from pathlib import Path

from tickline_tools import (
    AnalysisReportError,
    FindingReview,
    OutcomeStatistics,
    analyze_investigation,
    build_analysis_report,
    build_investigation_baseline,
    evaluate_investigation_outliers,
    load_investigation_bundle,
    render_analysis_report_json,
    write_analysis_report_json,
)


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
FIXTURE_PATH = (
    REPOSITORY_ROOT
    / "unity"
    / "TicklineForensics"
    / "Assets"
    / "Tickline"
    / "Forensics"
    / "Tests"
    / "EditMode"
    / "Fixtures"
    / "valid-investigation-bundle.json"
)


class AnalysisReportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        bundle = load_investigation_bundle(FIXTURE_PATH)
        cls.base_statistics = analyze_investigation(bundle)
        cls.generated_at = datetime(
            2026,
            7,
            6,
            12,
            0,
            0,
            tzinfo=timezone.utc,
        )

    def statistics(
        self,
        identifier: int,
        *,
        commands: int,
        rejected: int,
        sessions: int,
        unique_ticks: int,
        tick_span: int,
    ):
        return replace(
            self.base_statistics,
            archive_digest=f"{identifier:064x}",
            outcomes=OutcomeStatistics(
                accepted=commands - rejected,
                rejected=rejected,
            ),
            session_count=sessions,
            unique_target_ticks=unique_ticks,
            tick_span=tick_span,
        )

    def build_inputs(self):
        samples = tuple(
            self.statistics(
                identifier,
                commands=10 + identifier,
                rejected=1,
                sessions=1,
                unique_ticks=5,
                tick_span=5,
            )
            for identifier in range(1, 6)
        )
        baseline = build_investigation_baseline(samples)
        candidate = self.statistics(
            100,
            commands=100,
            rejected=90,
            sessions=1,
            unique_ticks=5,
            tick_span=5,
        )
        outliers = evaluate_investigation_outliers(
            baseline,
            candidate,
        )
        return baseline, candidate, outliers

    def test_renders_deterministic_versioned_json(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        report = build_analysis_report(
            candidate,
            baseline,
            outliers,
            generated_at_utc=self.generated_at,
        )

        first = render_analysis_report_json(report)
        second = render_analysis_report_json(report)
        document = json.loads(first)

        self.assertEqual(first, second)
        self.assertTrue(first.endswith("\n"))
        self.assertEqual(document["schemaVersion"], 1)
        self.assertEqual(
            document["investigation"]["archiveDigest"],
            candidate.archive_digest,
        )
        self.assertTrue(
            document["outlierAnalysis"]["hasOutliers"]
        )

    def test_reports_unreviewed_findings(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        report = build_analysis_report(
            candidate,
            baseline,
            outliers,
            generated_at_utc=self.generated_at,
        )
        document = json.loads(
            render_analysis_report_json(report)
        )

        self.assertEqual(
            document["reviewSummary"][
                "reviewedFindingCount"
            ],
            0,
        )
        self.assertEqual(
            document["reviewSummary"][
                "unreviewedFindingCount"
            ],
            len(outliers.findings),
        )
        self.assertTrue(
            all(
                finding["reviewStatus"] == "unreviewed"
                for finding in document[
                    "outlierAnalysis"
                ]["findings"]
            )
        )

    def test_applies_false_positive_review(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        review = FindingReview(
            metric="command_count",
            disposition="false_positive",
            rationale="Controlled load-test traffic.",
            reviewer="security-review",
            reviewed_at_utc=self.generated_at,
            evidence_ordinals=(0, 1),
        )
        report = build_analysis_report(
            candidate,
            baseline,
            outliers,
            generated_at_utc=self.generated_at,
            reviews=(review,),
        )
        document = json.loads(
            render_analysis_report_json(report)
        )
        finding = next(
            item
            for item in document["outlierAnalysis"][
                "findings"
            ]
            if item["metric"] == "command_count"
        )

        self.assertEqual(
            finding["reviewStatus"],
            "false_positive",
        )
        self.assertEqual(
            finding["review"]["evidenceOrdinals"],
            [0, 1],
        )
        self.assertEqual(
            document["reviewSummary"][
                "falsePositiveCount"
            ],
            1,
        )

    def test_writes_report_file(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        report = build_analysis_report(
            candidate,
            baseline,
            outliers,
            generated_at_utc=self.generated_at,
        )

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            write_analysis_report_json(report, path)

            self.assertEqual(
                path.read_text(encoding="utf-8"),
                render_analysis_report_json(report),
            )

    def test_rejects_mismatched_archive(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        outliers = replace(
            outliers,
            archive_digest="f" * 64,
        )

        with self.assertRaises(AnalysisReportError) as context:
            build_analysis_report(
                candidate,
                baseline,
                outliers,
                generated_at_utc=self.generated_at,
            )

        self.assertEqual(
            context.exception.code,
            "report.archive_mismatch",
        )

    def test_rejects_duplicate_reviews(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        review = FindingReview(
            metric="command_count",
            disposition="needs_context",
            rationale="Requires operator context.",
            reviewer="reviewer",
            reviewed_at_utc=self.generated_at,
        )

        with self.assertRaises(AnalysisReportError) as context:
            build_analysis_report(
                candidate,
                baseline,
                outliers,
                generated_at_utc=self.generated_at,
                reviews=(review, review),
            )

        self.assertEqual(
            context.exception.code,
            "review.duplicate",
        )

    def test_rejects_review_for_unknown_finding(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        review = FindingReview(
            metric="unknown_metric",
            disposition="false_positive",
            rationale="Not applicable.",
            reviewer="reviewer",
            reviewed_at_utc=self.generated_at,
        )

        with self.assertRaises(AnalysisReportError) as context:
            build_analysis_report(
                candidate,
                baseline,
                outliers,
                generated_at_utc=self.generated_at,
                reviews=(review,),
            )

        self.assertEqual(
            context.exception.code,
            "review.unknown_finding",
        )

    def test_rejects_invalid_evidence_ordinal(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        review = FindingReview(
            metric="command_count",
            disposition="confirmed_anomaly",
            rationale="Confirmed during review.",
            reviewer="reviewer",
            reviewed_at_utc=self.generated_at,
            evidence_ordinals=(1000,),
        )

        with self.assertRaises(AnalysisReportError) as context:
            build_analysis_report(
                candidate,
                baseline,
                outliers,
                generated_at_utc=self.generated_at,
                reviews=(review,),
            )

        self.assertEqual(
            context.exception.code,
            "review.evidence_ordinal",
        )

    def test_rejects_review_after_report_generation(self) -> None:
        baseline, candidate, outliers = self.build_inputs()
        review = FindingReview(
            metric="command_count",
            disposition="confirmed_anomaly",
            rationale="Confirmed during review.",
            reviewer="reviewer",
            reviewed_at_utc=(
                self.generated_at + timedelta(seconds=1)
            ),
        )

        with self.assertRaises(AnalysisReportError) as context:
            build_analysis_report(
                candidate,
                baseline,
                outliers,
                generated_at_utc=self.generated_at,
                reviews=(review,),
            )

        self.assertEqual(
            context.exception.code,
            "review.after_report",
        )


if __name__ == "__main__":
    unittest.main()
