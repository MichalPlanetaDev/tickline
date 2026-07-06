"""Command-line interface for Tickline investigation analytics."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Sequence, TextIO

from .analysis_report import (
    AnalysisReportError,
    FindingReview,
    build_analysis_report,
    render_analysis_report_json,
)
from .analytics import analyze_investigation
from .baseline import (
    InvestigationBaselineError,
    OutlierPolicy,
    build_investigation_baseline,
    evaluate_investigation_outliers,
)
from .investigation_bundle import (
    InvestigationBundleValidationError,
    load_investigation_bundle,
)


class ReviewFileError(ValueError):
    """Raised when a finding-review document is malformed."""


class _DuplicateJsonKeyError(ValueError):
    pass


def _reject_duplicate_keys(
    pairs: list[tuple[str, Any]],
) -> dict[str, Any]:
    result: dict[str, Any] = {}

    for key, value in pairs:
        if key in result:
            raise _DuplicateJsonKeyError(
                f"duplicate JSON object key {key!r}"
            )

        result[key] = value

    return result


def _parse_utc_timestamp(value: str, field: str) -> datetime:
    try:
        parsed = datetime.strptime(
            value,
            "%Y-%m-%dT%H:%M:%SZ",
        )
    except ValueError as error:
        raise ValueError(
            f"{field} must use YYYY-MM-DDTHH:MM:SSZ"
        ) from error

    return parsed.replace(tzinfo=timezone.utc)


def _require_object(
    value: Any,
    path: str,
    *,
    required: set[str],
    optional: set[str] | None = None,
) -> dict[str, Any]:
    if type(value) is not dict:
        raise ReviewFileError(
            f"{path} must be a JSON object"
        )

    allowed = required | (optional or set())

    missing = sorted(required - value.keys())
    if missing:
        raise ReviewFileError(
            f"{path}.{missing[0]} is required"
        )

    unexpected = sorted(value.keys() - allowed)
    if unexpected:
        raise ReviewFileError(
            f"{path}.{unexpected[0]} is not supported"
        )

    return value


def _require_string(
    value: Any,
    path: str,
) -> str:
    if type(value) is not str:
        raise ReviewFileError(
            f"{path} must be a JSON string"
        )

    if not value.strip():
        raise ReviewFileError(
            f"{path} must not be empty"
        )

    return value


def _parse_review(
    value: Any,
    index: int,
) -> FindingReview:
    path = f"$.reviews[{index}]"
    data = _require_object(
        value,
        path,
        required={
            "metric",
            "disposition",
            "rationale",
            "reviewer",
            "reviewedAtUtc",
        },
        optional={"evidenceOrdinals"},
    )

    ordinals_value = data.get("evidenceOrdinals", [])

    if type(ordinals_value) is not list:
        raise ReviewFileError(
            f"{path}.evidenceOrdinals must be a JSON array"
        )

    ordinals: list[int] = []

    for ordinal_index, ordinal in enumerate(ordinals_value):
        ordinal_path = (
            f"{path}.evidenceOrdinals[{ordinal_index}]"
        )

        if type(ordinal) is not int or ordinal < 0:
            raise ReviewFileError(
                f"{ordinal_path} must be a nonnegative integer"
            )

        ordinals.append(ordinal)

    reviewed_at_text = _require_string(
        data["reviewedAtUtc"],
        f"{path}.reviewedAtUtc",
    )

    try:
        reviewed_at = _parse_utc_timestamp(
            reviewed_at_text,
            f"{path}.reviewedAtUtc",
        )
    except ValueError as error:
        raise ReviewFileError(str(error)) from error

    return FindingReview(
        metric=_require_string(
            data["metric"],
            f"{path}.metric",
        ),
        disposition=_require_string(
            data["disposition"],
            f"{path}.disposition",
        ),
        rationale=_require_string(
            data["rationale"],
            f"{path}.rationale",
        ),
        reviewer=_require_string(
            data["reviewer"],
            f"{path}.reviewer",
        ),
        reviewed_at_utc=reviewed_at,
        evidence_ordinals=tuple(ordinals),
    )


def load_finding_reviews(
    path: str | Path,
) -> tuple[FindingReview, ...]:
    try:
        document = json.loads(
            Path(path).read_text(encoding="utf-8"),
            object_pairs_hook=_reject_duplicate_keys,
        )
    except json.JSONDecodeError as error:
        raise ReviewFileError(
            f"invalid review JSON: {error}"
        ) from error
    except _DuplicateJsonKeyError as error:
        raise ReviewFileError(str(error)) from error

    data = _require_object(
        document,
        "$",
        required={"reviews"},
    )
    reviews = data["reviews"]

    if type(reviews) is not list:
        raise ReviewFileError(
            "$.reviews must be a JSON array"
        )

    return tuple(
        _parse_review(value, index)
        for index, value in enumerate(reviews)
    )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="tickline-analytics",
        description=(
            "Generate deterministic analytics reports from "
            "validated Tickline investigation bundles."
        ),
    )
    subcommands = parser.add_subparsers(
        dest="command",
        required=True,
    )

    report = subcommands.add_parser(
        "report",
        help="analyze one candidate against a baseline",
    )
    report.add_argument(
        "--candidate",
        required=True,
        type=Path,
        help="candidate investigation-bundle JSON file",
    )
    report.add_argument(
        "--baseline",
        required=True,
        action="append",
        type=Path,
        dest="baseline_paths",
        help=(
            "baseline investigation-bundle JSON file; "
            "repeat for each sample"
        ),
    )
    report.add_argument(
        "--generated-at",
        required=True,
        help="report timestamp in YYYY-MM-DDTHH:MM:SSZ",
    )
    report.add_argument(
        "--review-file",
        type=Path,
        help="optional JSON file containing finding reviews",
    )
    report.add_argument(
        "--output",
        type=Path,
        help="output file; omit to write JSON to stdout",
    )
    report.add_argument(
        "--minimum-samples",
        type=int,
        default=5,
        help="minimum accepted baseline sample count",
    )
    report.add_argument(
        "--modified-z-threshold",
        type=float,
        default=3.5,
        help="modified z-score threshold",
    )

    return parser


def _run_report(
    arguments: argparse.Namespace,
    stdout: TextIO,
) -> int:
    generated_at = _parse_utc_timestamp(
        arguments.generated_at,
        "--generated-at",
    )

    baseline_statistics = tuple(
        analyze_investigation(
            load_investigation_bundle(path)
        )
        for path in arguments.baseline_paths
    )
    candidate_statistics = analyze_investigation(
        load_investigation_bundle(arguments.candidate)
    )

    policy = OutlierPolicy(
        minimum_samples=arguments.minimum_samples,
        modified_z_threshold=(
            arguments.modified_z_threshold
        ),
    )
    baseline = build_investigation_baseline(
        baseline_statistics,
        policy,
    )
    outliers = evaluate_investigation_outliers(
        baseline,
        candidate_statistics,
    )

    reviews = (
        load_finding_reviews(arguments.review_file)
        if arguments.review_file is not None
        else ()
    )

    report = build_analysis_report(
        candidate_statistics,
        baseline,
        outliers,
        generated_at_utc=generated_at,
        reviews=reviews,
    )
    rendered = render_analysis_report_json(report)

    if arguments.output is None:
        stdout.write(rendered)
    else:
        arguments.output.write_text(
            rendered,
            encoding="utf-8",
        )

    return 0


def main(
    argv: Sequence[str] | None = None,
    *,
    stdout: TextIO | None = None,
    stderr: TextIO | None = None,
) -> int:
    output = stdout or sys.stdout
    error_output = stderr or sys.stderr
    parser = _build_parser()
    arguments = parser.parse_args(argv)

    try:
        if arguments.command == "report":
            return _run_report(arguments, output)

        parser.error(
            f"unsupported command {arguments.command!r}"
        )
    except (
        AnalysisReportError,
        InvestigationBaselineError,
        InvestigationBundleValidationError,
        ReviewFileError,
        OSError,
        ValueError,
    ) as error:
        print(f"error: {error}", file=error_output)
        return 2

    return 2
