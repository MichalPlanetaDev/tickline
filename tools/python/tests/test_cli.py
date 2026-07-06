import json
import tempfile
import unittest
from io import StringIO
from pathlib import Path

from tickline_tools.cli import main


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


class CliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.fixture = json.loads(
            FIXTURE_PATH.read_text(encoding="utf-8")
        )

    def write_bundle(
        self,
        directory: Path,
        identifier: int,
        *,
        outlier: bool = False,
    ) -> Path:
        document = json.loads(json.dumps(self.fixture))
        document["archiveDigest"] = f"{identifier:064x}"

        if outlier:
            records = []
            previous_digest = document[
                "initialHeadDigest"
            ]

            for index in range(10):
                record_digest = f"{1000 + index:064x}"
                records.append(
                    {
                        "ordinal": index,
                        "clientId": "7",
                        "sessionId": "11",
                        "sessionSequence": str(index + 1),
                        "targetTick": str(index + 1),
                        "commandType": "set_velocity",
                        "outcome": "accepted",
                        "rejectionCode": "none",
                        "previousDigest": previous_digest,
                        "recordDigest": record_digest,
                    }
                )
                previous_digest = record_digest

            document["trustedHeadDigest"] = previous_digest
            document["evidence"] = records
            document["sessions"][0].update(
                {
                    "lastCommittedSequence": "10",
                    "firstTargetTick": "1",
                    "lastTargetTick": "10",
                    "acceptedCommands": 10,
                    "rejectedCommands": 0,
                }
            )
            document["replay"].update(
                {
                    "finalTick": "10",
                    "acceptedCommands": 10,
                    "rejectedCommands": 0,
                }
            )

        path = directory / f"bundle-{identifier}.json"
        path.write_text(
            json.dumps(document),
            encoding="utf-8",
        )
        return path

    def report_arguments(
        self,
        candidate: Path,
        baselines: list[Path],
    ) -> list[str]:
        arguments = [
            "report",
            "--candidate",
            str(candidate),
            "--generated-at",
            "2026-07-06T12:00:00Z",
        ]

        for baseline in baselines:
            arguments.extend(
                ["--baseline", str(baseline)]
            )

        return arguments

    def create_inputs(
        self,
        directory: Path,
        *,
        outlier: bool = False,
    ) -> tuple[list[Path], Path]:
        baselines = [
            self.write_bundle(directory, identifier)
            for identifier in range(1, 6)
        ]
        candidate = self.write_bundle(
            directory,
            100,
            outlier=outlier,
        )
        return baselines, candidate

    def test_writes_report_to_stdout(self) -> None:
        with tempfile.TemporaryDirectory() as raw_directory:
            directory = Path(raw_directory)
            baselines, candidate = self.create_inputs(
                directory
            )
            stdout = StringIO()
            stderr = StringIO()

            result = main(
                self.report_arguments(
                    candidate,
                    baselines,
                ),
                stdout=stdout,
                stderr=stderr,
            )

            document = json.loads(stdout.getvalue())

            self.assertEqual(result, 0)
            self.assertEqual(stderr.getvalue(), "")
            self.assertEqual(document["schemaVersion"], 1)
            self.assertEqual(
                document["investigation"]["archiveDigest"],
                f"{100:064x}",
            )
            self.assertFalse(
                document["outlierAnalysis"]["hasOutliers"]
            )

    def test_writes_report_to_file(self) -> None:
        with tempfile.TemporaryDirectory() as raw_directory:
            directory = Path(raw_directory)
            baselines, candidate = self.create_inputs(
                directory
            )
            output_path = directory / "report.json"
            arguments = self.report_arguments(
                candidate,
                baselines,
            )
            arguments.extend(
                ["--output", str(output_path)]
            )
            stdout = StringIO()
            stderr = StringIO()

            result = main(
                arguments,
                stdout=stdout,
                stderr=stderr,
            )

            self.assertEqual(result, 0)
            self.assertEqual(stdout.getvalue(), "")
            self.assertEqual(stderr.getvalue(), "")
            self.assertTrue(output_path.is_file())
            self.assertEqual(
                json.loads(
                    output_path.read_text(
                        encoding="utf-8"
                    )
                )["schemaVersion"],
                1,
            )

    def test_applies_review_file(self) -> None:
        with tempfile.TemporaryDirectory() as raw_directory:
            directory = Path(raw_directory)
            baselines, candidate = self.create_inputs(
                directory,
                outlier=True,
            )
            review_path = directory / "reviews.json"
            review_path.write_text(
                json.dumps(
                    {
                        "reviews": [
                            {
                                "metric": "command_count",
                                "disposition": "false_positive",
                                "rationale": (
                                    "Controlled load-test traffic."
                                ),
                                "reviewer": "security-review",
                                "reviewedAtUtc": (
                                    "2026-07-06T12:00:00Z"
                                ),
                                "evidenceOrdinals": [0],
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            arguments = self.report_arguments(
                candidate,
                baselines,
            )
            arguments.extend(
                ["--review-file", str(review_path)]
            )
            stdout = StringIO()

            result = main(
                arguments,
                stdout=stdout,
                stderr=StringIO(),
            )
            document = json.loads(stdout.getvalue())
            finding = next(
                item
                for item in document[
                    "outlierAnalysis"
                ]["findings"]
                if item["metric"] == "command_count"
            )

            self.assertEqual(result, 0)
            self.assertEqual(
                finding["reviewStatus"],
                "false_positive",
            )
            self.assertEqual(
                document["reviewSummary"][
                    "falsePositiveCount"
                ],
                1,
            )

    def test_rejects_invalid_generated_timestamp(self) -> None:
        with tempfile.TemporaryDirectory() as raw_directory:
            directory = Path(raw_directory)
            baselines, candidate = self.create_inputs(
                directory
            )
            arguments = self.report_arguments(
                candidate,
                baselines,
            )
            timestamp_index = arguments.index(
                "--generated-at"
            ) + 1
            arguments[timestamp_index] = "2026-07-06"
            stderr = StringIO()

            result = main(
                arguments,
                stdout=StringIO(),
                stderr=stderr,
            )

            self.assertEqual(result, 2)
            self.assertIn(
                "--generated-at must use",
                stderr.getvalue(),
            )

    def test_rejects_insufficient_baseline(self) -> None:
        with tempfile.TemporaryDirectory() as raw_directory:
            directory = Path(raw_directory)
            baselines, candidate = self.create_inputs(
                directory
            )
            stderr = StringIO()

            result = main(
                self.report_arguments(
                    candidate,
                    baselines[:4],
                ),
                stdout=StringIO(),
                stderr=stderr,
            )

            self.assertEqual(result, 2)
            self.assertIn(
                "baseline.insufficient_samples",
                stderr.getvalue(),
            )

    def test_rejects_candidate_in_baseline(self) -> None:
        with tempfile.TemporaryDirectory() as raw_directory:
            directory = Path(raw_directory)
            baselines, _ = self.create_inputs(directory)
            stderr = StringIO()

            result = main(
                self.report_arguments(
                    baselines[0],
                    baselines,
                ),
                stdout=StringIO(),
                stderr=stderr,
            )

            self.assertEqual(result, 2)
            self.assertIn(
                "candidate.in_baseline",
                stderr.getvalue(),
            )

    def test_rejects_malformed_review_file(self) -> None:
        with tempfile.TemporaryDirectory() as raw_directory:
            directory = Path(raw_directory)
            baselines, candidate = self.create_inputs(
                directory
            )
            review_path = directory / "reviews.json"
            review_path.write_text(
                '{"reviews":[],"unexpected":true}',
                encoding="utf-8",
            )
            arguments = self.report_arguments(
                candidate,
                baselines,
            )
            arguments.extend(
                ["--review-file", str(review_path)]
            )
            stderr = StringIO()

            result = main(
                arguments,
                stdout=StringIO(),
                stderr=stderr,
            )

            self.assertEqual(result, 2)
            self.assertIn(
                "$.unexpected is not supported",
                stderr.getvalue(),
            )

    def test_rejects_invalid_candidate_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as raw_directory:
            directory = Path(raw_directory)
            baselines, candidate = self.create_inputs(
                directory
            )
            document = json.loads(
                candidate.read_text(encoding="utf-8")
            )
            document["schemaVersion"] = 2
            candidate.write_text(
                json.dumps(document),
                encoding="utf-8",
            )
            stderr = StringIO()

            result = main(
                self.report_arguments(
                    candidate,
                    baselines,
                ),
                stdout=StringIO(),
                stderr=stderr,
            )

            self.assertEqual(result, 2)
            self.assertIn(
                "schema.unsupported",
                stderr.getvalue(),
            )


if __name__ == "__main__":
    unittest.main()
