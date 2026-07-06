import unittest
from dataclasses import replace
from pathlib import Path

from tickline_tools import (
    InvestigationBaselineError,
    OutcomeStatistics,
    OutlierPolicy,
    analyze_investigation,
    build_investigation_baseline,
    evaluate_investigation_outliers,
    load_investigation_bundle,
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


class BaselineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        bundle = load_investigation_bundle(FIXTURE_PATH)
        cls.base_statistics = analyze_investigation(bundle)

    def statistics(
        self,
        identifier: int,
        *,
        commands: int,
        rejected: int,
        sessions: int,
        unique_ticks: int,
        tick_span: int,
        verified: bool = True,
    ):
        return replace(
            self.base_statistics,
            archive_digest=f"{identifier:064x}",
            replay_verified=verified,
            outcomes=OutcomeStatistics(
                accepted=commands - rejected,
                rejected=rejected,
            ),
            session_count=sessions,
            unique_target_ticks=unique_ticks,
            tick_span=tick_span,
        )

    def varied_samples(self):
        return (
            self.statistics(
                1,
                commands=10,
                rejected=1,
                sessions=1,
                unique_ticks=5,
                tick_span=5,
            ),
            self.statistics(
                2,
                commands=11,
                rejected=1,
                sessions=1,
                unique_ticks=5,
                tick_span=5,
            ),
            self.statistics(
                3,
                commands=12,
                rejected=2,
                sessions=1,
                unique_ticks=6,
                tick_span=6,
            ),
            self.statistics(
                4,
                commands=13,
                rejected=2,
                sessions=2,
                unique_ticks=6,
                tick_span=6,
            ),
            self.statistics(
                5,
                commands=14,
                rejected=3,
                sessions=2,
                unique_ticks=7,
                tick_span=7,
            ),
        )

    def test_policy_rejects_invalid_thresholds(self) -> None:
        with self.assertRaises(ValueError):
            OutlierPolicy(minimum_samples=2)

        with self.assertRaises(ValueError):
            OutlierPolicy(modified_z_threshold=0)

        with self.assertRaises(ValueError):
            OutlierPolicy(rejection_rate_tolerance=0)

    def test_rejects_insufficient_baseline(self) -> None:
        with self.assertRaises(
            InvestigationBaselineError
        ) as context:
            build_investigation_baseline(
                self.varied_samples()[:4]
            )

        self.assertEqual(
            context.exception.code,
            "baseline.insufficient_samples",
        )

    def test_rejects_unverified_baseline_sample(self) -> None:
        samples = list(self.varied_samples())
        samples[-1] = replace(
            samples[-1],
            replay_verified=False,
        )

        with self.assertRaises(
            InvestigationBaselineError
        ) as context:
            build_investigation_baseline(samples)

        self.assertEqual(
            context.exception.code,
            "baseline.unverified",
        )

    def test_rejects_duplicate_baseline_archive(self) -> None:
        samples = list(self.varied_samples())
        samples[-1] = replace(
            samples[-1],
            archive_digest=samples[0].archive_digest,
        )

        with self.assertRaises(
            InvestigationBaselineError
        ) as context:
            build_investigation_baseline(samples)

        self.assertEqual(
            context.exception.code,
            "baseline.duplicate_archive",
        )

    def test_builds_deterministic_metric_baseline(self) -> None:
        baseline = build_investigation_baseline(
            self.varied_samples()
        )

        self.assertEqual(baseline.sample_count, 5)
        self.assertEqual(
            tuple(item.metric for item in baseline.metrics),
            (
                "command_count",
                "rejection_rate",
                "session_count",
                "unique_target_ticks",
                "tick_span",
                "commands_per_tick",
            ),
        )

        command_count = baseline.metric("command_count")

        self.assertEqual(command_count.minimum, 10.0)
        self.assertEqual(command_count.maximum, 14.0)
        self.assertEqual(command_count.mean, 12.0)
        self.assertEqual(command_count.median, 12.0)
        self.assertEqual(
            command_count.median_absolute_deviation,
            1.0,
        )

        second = build_investigation_baseline(
            reversed(self.varied_samples())
        )
        self.assertEqual(baseline, second)

    def test_baseline_like_candidate_has_no_outliers(self) -> None:
        baseline = build_investigation_baseline(
            self.varied_samples()
        )
        candidate = self.statistics(
            100,
            commands=12,
            rejected=2,
            sessions=1,
            unique_ticks=6,
            tick_span=6,
        )

        analysis = evaluate_investigation_outliers(
            baseline,
            candidate,
        )

        self.assertFalse(analysis.has_outliers)
        self.assertEqual(analysis.findings, ())
        self.assertEqual(analysis.baseline_sample_count, 5)

    def test_detects_modified_z_outliers(self) -> None:
        baseline = build_investigation_baseline(
            self.varied_samples()
        )
        candidate = self.statistics(
            101,
            commands=100,
            rejected=90,
            sessions=1,
            unique_ticks=6,
            tick_span=6,
        )

        analysis = evaluate_investigation_outliers(
            baseline,
            candidate,
        )
        findings = {
            finding.metric: finding
            for finding in analysis.findings
        }

        self.assertTrue(analysis.has_outliers)
        self.assertIn("command_count", findings)
        self.assertIn("rejection_rate", findings)
        self.assertIn("commands_per_tick", findings)
        self.assertEqual(
            findings["command_count"].method,
            "modified_z",
        )
        self.assertEqual(
            findings["command_count"].direction,
            "high",
        )
        self.assertGreaterEqual(
            findings["command_count"].score,
            findings["command_count"].threshold,
        )

    def test_uses_explicit_tolerance_when_mad_is_zero(self) -> None:
        samples = tuple(
            self.statistics(
                identifier,
                commands=10,
                rejected=1,
                sessions=1,
                unique_ticks=5,
                tick_span=5,
            )
            for identifier in range(1, 6)
        )
        baseline = build_investigation_baseline(samples)
        candidate = self.statistics(
            200,
            commands=13,
            rejected=1,
            sessions=1,
            unique_ticks=5,
            tick_span=5,
        )

        analysis = evaluate_investigation_outliers(
            baseline,
            candidate,
        )
        command_finding = next(
            finding
            for finding in analysis.findings
            if finding.metric == "command_count"
        )

        self.assertEqual(
            command_finding.method,
            "zero_mad_tolerance",
        )
        self.assertEqual(command_finding.score, 3.0)
        self.assertEqual(command_finding.threshold, 1.0)

    def test_rejects_candidate_already_in_baseline(self) -> None:
        samples = self.varied_samples()
        baseline = build_investigation_baseline(samples)

        with self.assertRaises(
            InvestigationBaselineError
        ) as context:
            evaluate_investigation_outliers(
                baseline,
                samples[0],
            )

        self.assertEqual(
            context.exception.code,
            "candidate.in_baseline",
        )

    def test_rejects_unverified_candidate(self) -> None:
        baseline = build_investigation_baseline(
            self.varied_samples()
        )
        candidate = self.statistics(
            300,
            commands=12,
            rejected=2,
            sessions=1,
            unique_ticks=6,
            tick_span=6,
            verified=False,
        )

        with self.assertRaises(
            InvestigationBaselineError
        ) as context:
            evaluate_investigation_outliers(
                baseline,
                candidate,
            )

        self.assertEqual(
            context.exception.code,
            "candidate.unverified",
        )


if __name__ == "__main__":
    unittest.main()
