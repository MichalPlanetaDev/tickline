import json
import unittest
from pathlib import Path

from tickline_tools import (
    InvestigationBundleValidationError,
    load_investigation_bundle,
    parse_investigation_bundle_json,
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


class InvestigationBundleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.fixture_text = FIXTURE_PATH.read_text(encoding="utf-8")

    def payload(self) -> dict:
        return json.loads(self.fixture_text)

    def assert_validation_code(
        self,
        payload: dict,
        expected_code: str,
    ) -> InvestigationBundleValidationError:
        with self.assertRaises(
            InvestigationBundleValidationError
        ) as context:
            parse_investigation_bundle_json(
                json.dumps(payload)
            )

        self.assertEqual(context.exception.code, expected_code)
        return context.exception

    def test_loads_native_generated_fixture(self) -> None:
        bundle = load_investigation_bundle(FIXTURE_PATH)

        self.assertEqual(bundle.schema_version, 1)
        self.assertEqual(bundle.total_commands, 4)
        self.assertEqual(len(bundle.sessions), 1)
        self.assertEqual(bundle.sessions[0].key, (7, 11))
        self.assertEqual(bundle.sessions[0].accepted_commands, 2)
        self.assertEqual(bundle.sessions[0].rejected_commands, 2)
        self.assertEqual(bundle.evidence[0].ordinal, 0)
        self.assertTrue(bundle.evidence[0].is_accepted)
        self.assertEqual(
            bundle.evidence[1].rejection_code,
            "duplicate_sequence",
        )
        self.assertTrue(bundle.replay.verified)
        self.assertEqual(bundle.replay.final_tick, 3)
        self.assertEqual(
            bundle.replay.final_world_fingerprint,
            72_623_859_790_382_856,
        )

    def test_rejects_duplicate_json_keys(self) -> None:
        with self.assertRaises(
            InvestigationBundleValidationError
        ) as context:
            parse_investigation_bundle_json(
                '{"schemaVersion":1,"schemaVersion":1}'
            )

        self.assertEqual(context.exception.code, "json.invalid")
        self.assertEqual(context.exception.path, "$")

    def test_rejects_unsupported_schema_version(self) -> None:
        payload = self.payload()
        payload["schemaVersion"] = 2

        error = self.assert_validation_code(
            payload,
            "schema.unsupported",
        )
        self.assertEqual(error.path, "$.schemaVersion")

    def test_rejects_non_string_uint64_value(self) -> None:
        payload = self.payload()
        payload["sessions"][0]["clientId"] = 7

        error = self.assert_validation_code(
            payload,
            "type.string",
        )
        self.assertEqual(
            error.path,
            "$.sessions[0].clientId",
        )

    def test_rejects_unexpected_field(self) -> None:
        payload = self.payload()
        payload["untrustedExtension"] = True

        error = self.assert_validation_code(
            payload,
            "field.unexpected",
        )
        self.assertEqual(
            error.path,
            "$.untrustedExtension",
        )

    def test_rejects_broken_evidence_chain(self) -> None:
        payload = self.payload()
        payload["evidence"][1]["previousDigest"] = "0" * 64

        error = self.assert_validation_code(
            payload,
            "evidence.chain_discontinuity",
        )
        self.assertEqual(
            error.path,
            "$.evidence[1].previousDigest",
        )

    def test_rejects_incorrect_session_summary(self) -> None:
        payload = self.payload()
        payload["sessions"][0]["acceptedCommands"] = 3

        error = self.assert_validation_code(
            payload,
            "session.accepted_count",
        )
        self.assertEqual(
            error.path,
            "$.sessions[0].acceptedCommands",
        )

    def test_rejects_incorrect_replay_summary(self) -> None:
        payload = self.payload()
        payload["replay"]["rejectedCommands"] = 1

        error = self.assert_validation_code(
            payload,
            "replay.rejected_count",
        )
        self.assertEqual(
            error.path,
            "$.replay.rejectedCommands",
        )

    def test_rejects_rejection_code_on_accepted_record(self) -> None:
        payload = self.payload()
        payload["evidence"][0]["rejectionCode"] = (
            "duplicate_sequence"
        )

        error = self.assert_validation_code(
            payload,
            "evidence.accepted_rejection_code",
        )
        self.assertEqual(
            error.path,
            "$.evidence[0].rejectionCode",
        )


if __name__ == "__main__":
    unittest.main()
