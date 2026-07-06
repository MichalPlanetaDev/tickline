import unittest
from datetime import datetime, timezone
from pathlib import Path

from tickline_tools import (
    InvestigationBundle,
    OutcomeStatistics,
    ReplaySummary,
    analyze_investigation,
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


class AnalyticsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.bundle = load_investigation_bundle(FIXTURE_PATH)
        cls.statistics = analyze_investigation(cls.bundle)

    def test_computes_overall_outcomes(self) -> None:
        outcomes = self.statistics.outcomes

        self.assertEqual(outcomes.total, 4)
        self.assertEqual(outcomes.accepted, 2)
        self.assertEqual(outcomes.rejected, 2)
        self.assertEqual(outcomes.acceptance_rate, 0.5)
        self.assertEqual(outcomes.rejection_rate, 0.5)

    def test_computes_session_statistics(self) -> None:
        self.assertEqual(self.statistics.session_count, 1)
        self.assertEqual(self.statistics.unique_clients, 1)

        session = self.statistics.sessions[0]

        self.assertEqual(session.key, (7, 11))
        self.assertEqual(session.last_committed_sequence, 2)
        self.assertEqual(session.first_target_tick, 2)
        self.assertEqual(session.last_target_tick, 3)
        self.assertEqual(session.tick_span, 2)
        self.assertEqual(session.unique_target_ticks, 2)
        self.assertEqual(session.first_ordinal, 0)
        self.assertEqual(session.last_ordinal, 3)
        self.assertEqual(
            session.command_types,
            ("set_velocity",),
        )
        self.assertEqual(
            session.outcomes,
            OutcomeStatistics(accepted=2, rejected=2),
        )

    def test_computes_command_type_statistics(self) -> None:
        self.assertEqual(len(self.statistics.command_types), 1)

        command = self.statistics.command_types[0]

        self.assertEqual(command.command_type, "set_velocity")
        self.assertEqual(command.outcomes.total, 4)
        self.assertEqual(command.outcomes.accepted, 2)
        self.assertEqual(command.outcomes.rejected, 2)
        self.assertEqual(command.session_count, 1)
        self.assertEqual(command.first_target_tick, 2)
        self.assertEqual(command.last_target_tick, 3)

    def test_ranks_rejection_codes_deterministically(self) -> None:
        self.assertEqual(
            tuple(
                item.rejection_code
                for item in self.statistics.rejection_codes
            ),
            ("duplicate_sequence", "unknown_entity"),
        )
        self.assertEqual(
            tuple(
                item.count
                for item in self.statistics.rejection_codes
            ),
            (1, 1),
        )

    def test_computes_tick_statistics(self) -> None:
        self.assertEqual(self.statistics.unique_target_ticks, 2)
        self.assertEqual(self.statistics.first_target_tick, 2)
        self.assertEqual(self.statistics.last_target_tick, 3)
        self.assertEqual(self.statistics.tick_span, 2)
        self.assertEqual(self.statistics.busiest_ticks, (3,))

        first_tick, second_tick = self.statistics.ticks

        self.assertEqual(first_tick.target_tick, 2)
        self.assertEqual(first_tick.outcomes.accepted, 1)
        self.assertEqual(first_tick.outcomes.rejected, 0)

        self.assertEqual(second_tick.target_tick, 3)
        self.assertEqual(second_tick.outcomes.accepted, 1)
        self.assertEqual(second_tick.outcomes.rejected, 2)

    def test_preserves_replay_metadata(self) -> None:
        self.assertTrue(self.statistics.replay_verified)
        self.assertEqual(self.statistics.final_tick, 3)
        self.assertEqual(
            self.statistics.final_world_fingerprint,
            72_623_859_790_382_856,
        )

    def test_analysis_is_deterministic(self) -> None:
        first = analyze_investigation(self.bundle)
        second = analyze_investigation(self.bundle)

        self.assertEqual(first, second)

    def test_empty_bundle_produces_zero_statistics(self) -> None:
        digest = "0" * 64
        bundle = InvestigationBundle(
            schema_version=1,
            archive_digest=digest,
            initial_head_digest=digest,
            trusted_head_digest=digest,
            imported_at_utc=datetime(
                2026,
                7,
                6,
                tzinfo=timezone.utc,
            ),
            sessions=(),
            evidence=(),
            replay=ReplaySummary(
                verified=True,
                final_tick=0,
                final_world_fingerprint=0,
                accepted_commands=0,
                rejected_commands=0,
            ),
        )

        statistics = analyze_investigation(bundle)

        self.assertEqual(statistics.outcomes.total, 0)
        self.assertEqual(statistics.outcomes.acceptance_rate, 0.0)
        self.assertEqual(statistics.outcomes.rejection_rate, 0.0)
        self.assertEqual(statistics.session_count, 0)
        self.assertEqual(statistics.unique_clients, 0)
        self.assertEqual(statistics.unique_target_ticks, 0)
        self.assertIsNone(statistics.first_target_tick)
        self.assertIsNone(statistics.last_target_tick)
        self.assertEqual(statistics.tick_span, 0)
        self.assertEqual(statistics.busiest_ticks, ())
        self.assertEqual(statistics.sessions, ())
        self.assertEqual(statistics.command_types, ())
        self.assertEqual(statistics.rejection_codes, ())
        self.assertEqual(statistics.ticks, ())


if __name__ == "__main__":
    unittest.main()
