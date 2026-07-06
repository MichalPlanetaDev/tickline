"""Versioned machine-readable investigation analytics reports."""

from __future__ import annotations

import json
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Iterable

from .analytics import InvestigationStatistics
from .baseline import (
    InvestigationBaseline,
    OutlierAnalysis,
    OutlierFinding,
)

ANALYSIS_REPORT_SCHEMA_VERSION = 1

REVIEW_DISPOSITIONS = frozenset(
    {
        "confirmed_anomaly",
        "expected_behavior",
        "false_positive",
        "needs_context",
    }
)


class AnalysisReportError(ValueError):
    """Raised when report inputs violate the reporting contract."""

    def __init__(self, code: str, message: str) -> None:
        self.code = code
        self.message = message
        super().__init__(f"{code}: {message}")


@dataclass(frozen=True, slots=True)
class FindingReview:
    metric: str
    disposition: str
    rationale: str
    reviewer: str
    reviewed_at_utc: datetime
    evidence_ordinals: tuple[int, ...] = ()


@dataclass(frozen=True, slots=True)
class InvestigationAnalysisReport:
    schema_version: int
    generated_at_utc: datetime
    statistics: InvestigationStatistics
    baseline: InvestigationBaseline
    outliers: OutlierAnalysis
    reviews: tuple[FindingReview, ...]

    @property
    def reviewed_finding_count(self) -> int:
        return len(self.reviews)

    @property
    def unreviewed_finding_count(self) -> int:
        return len(self.outliers.findings) - len(self.reviews)

    @property
    def false_positive_count(self) -> int:
        return sum(
            review.disposition == "false_positive"
            for review in self.reviews
        )


def _fail(code: str, message: str) -> None:
    raise AnalysisReportError(code, message)


def _is_utc(value: datetime) -> bool:
    return (
        value.tzinfo is not None
        and value.utcoffset() == timedelta(0)
    )


def _format_utc(value: datetime) -> str:
    return value.astimezone(timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ"
    )


def _number(value: float) -> float:
    return round(value, 12)


def _validate_review(
    review: FindingReview,
    finding_metrics: set[str],
    evidence_count: int,
    generated_at_utc: datetime,
) -> None:
    if not review.metric.strip():
        _fail(
            "review.metric_empty",
            "review metric must not be empty",
        )

    if review.metric not in finding_metrics:
        _fail(
            "review.unknown_finding",
            (
                f"review metric {review.metric!r} does not "
                "identify an outlier finding"
            ),
        )

    if review.disposition not in REVIEW_DISPOSITIONS:
        _fail(
            "review.invalid_disposition",
            (
                f"unsupported review disposition "
                f"{review.disposition!r}"
            ),
        )

    if not review.rationale.strip():
        _fail(
            "review.rationale_empty",
            "review rationale must not be empty",
        )

    if not review.reviewer.strip():
        _fail(
            "review.reviewer_empty",
            "reviewer must not be empty",
        )

    if not _is_utc(review.reviewed_at_utc):
        _fail(
            "review.timestamp_not_utc",
            "review timestamp must use UTC",
        )

    if review.reviewed_at_utc > generated_at_utc:
        _fail(
            "review.after_report",
            "review timestamp must not follow report generation",
        )

    canonical_ordinals = tuple(
        sorted(set(review.evidence_ordinals))
    )

    if canonical_ordinals != review.evidence_ordinals:
        _fail(
            "review.evidence_order",
            (
                "evidence ordinals must be unique and "
                "strictly increasing"
            ),
        )

    for ordinal in review.evidence_ordinals:
        if type(ordinal) is not int or not 0 <= ordinal < evidence_count:
            _fail(
                "review.evidence_ordinal",
                (
                    f"evidence ordinal {ordinal!r} is outside "
                    f"the available range 0..{evidence_count - 1}"
                ),
            )


def build_analysis_report(
    statistics: InvestigationStatistics,
    baseline: InvestigationBaseline,
    outliers: OutlierAnalysis,
    *,
    generated_at_utc: datetime,
    reviews: Iterable[FindingReview] = (),
) -> InvestigationAnalysisReport:
    if not _is_utc(generated_at_utc):
        _fail(
            "report.timestamp_not_utc",
            "report generation timestamp must use UTC",
        )

    if outliers.archive_digest != statistics.archive_digest:
        _fail(
            "report.archive_mismatch",
            (
                "outlier analysis archive digest does not match "
                "the investigation statistics"
            ),
        )

    if outliers.baseline_sample_count != baseline.sample_count:
        _fail(
            "report.baseline_mismatch",
            (
                "outlier analysis baseline sample count does not "
                "match the supplied baseline"
            ),
        )

    finding_metrics = {
        finding.metric for finding in outliers.findings
    }

    if len(finding_metrics) != len(outliers.findings):
        _fail(
            "report.duplicate_finding",
            "outlier analysis contains duplicate metric findings",
        )

    review_items = tuple(reviews)
    reviewed_metrics: set[str] = set()

    for review in review_items:
        if review.metric in reviewed_metrics:
            _fail(
                "review.duplicate",
                (
                    f"metric {review.metric!r} has more than "
                    "one review"
                ),
            )

        _validate_review(
            review,
            finding_metrics,
            statistics.outcomes.total,
            generated_at_utc,
        )
        reviewed_metrics.add(review.metric)

    review_items = tuple(
        sorted(review_items, key=lambda item: item.metric)
    )

    return InvestigationAnalysisReport(
        schema_version=ANALYSIS_REPORT_SCHEMA_VERSION,
        generated_at_utc=generated_at_utc,
        statistics=statistics,
        baseline=baseline,
        outliers=outliers,
        reviews=review_items,
    )


def _outcomes_document(outcomes: Any) -> dict[str, Any]:
    return {
        "accepted": outcomes.accepted,
        "rejected": outcomes.rejected,
        "total": outcomes.total,
        "acceptanceRate": _number(outcomes.acceptance_rate),
        "rejectionRate": _number(outcomes.rejection_rate),
    }


def _finding_document(
    finding: OutlierFinding,
    review: FindingReview | None,
) -> dict[str, Any]:
    review_document: dict[str, Any] | None = None

    if review is not None:
        review_document = {
            "disposition": review.disposition,
            "rationale": review.rationale,
            "reviewer": review.reviewer,
            "reviewedAtUtc": _format_utc(
                review.reviewed_at_utc
            ),
            "evidenceOrdinals": list(
                review.evidence_ordinals
            ),
        }

    return {
        "metric": finding.metric,
        "direction": finding.direction,
        "observed": _number(finding.observed),
        "baselineMedian": _number(
            finding.baseline_median
        ),
        "baselineMad": _number(finding.baseline_mad),
        "score": _number(finding.score),
        "threshold": _number(finding.threshold),
        "normalizedSeverity": _number(
            finding.normalized_severity
        ),
        "method": finding.method,
        "explanation": finding.explanation,
        "reviewStatus": (
            review.disposition
            if review is not None
            else "unreviewed"
        ),
        "review": review_document,
    }


def analysis_report_document(
    report: InvestigationAnalysisReport,
) -> dict[str, Any]:
    statistics = report.statistics
    baseline = report.baseline
    reviews_by_metric = {
        review.metric: review for review in report.reviews
    }

    return {
        "schemaVersion": report.schema_version,
        "generatedAtUtc": _format_utc(
            report.generated_at_utc
        ),
        "investigation": {
            "archiveDigest": statistics.archive_digest,
            "importedAtUtc": _format_utc(
                statistics.imported_at_utc
            ),
            "replayVerified": statistics.replay_verified,
            "finalTick": str(statistics.final_tick),
            "finalWorldFingerprint": str(
                statistics.final_world_fingerprint
            ),
            "outcomes": _outcomes_document(
                statistics.outcomes
            ),
            "sessionCount": statistics.session_count,
            "uniqueClients": statistics.unique_clients,
            "uniqueTargetTicks": (
                statistics.unique_target_ticks
            ),
            "firstTargetTick": (
                str(statistics.first_target_tick)
                if statistics.first_target_tick is not None
                else None
            ),
            "lastTargetTick": (
                str(statistics.last_target_tick)
                if statistics.last_target_tick is not None
                else None
            ),
            "tickSpan": str(statistics.tick_span),
            "busiestTicks": [
                str(value)
                for value in statistics.busiest_ticks
            ],
            "sessions": [
                {
                    "clientId": str(session.client_id),
                    "sessionId": str(session.session_id),
                    "lastCommittedSequence": str(
                        session.last_committed_sequence
                    ),
                    "firstTargetTick": str(
                        session.first_target_tick
                    ),
                    "lastTargetTick": str(
                        session.last_target_tick
                    ),
                    "tickSpan": str(session.tick_span),
                    "uniqueTargetTicks": (
                        session.unique_target_ticks
                    ),
                    "firstOrdinal": session.first_ordinal,
                    "lastOrdinal": session.last_ordinal,
                    "commandTypes": list(
                        session.command_types
                    ),
                    "outcomes": _outcomes_document(
                        session.outcomes
                    ),
                }
                for session in statistics.sessions
            ],
            "commandTypes": [
                {
                    "commandType": item.command_type,
                    "sessionCount": item.session_count,
                    "firstTargetTick": str(
                        item.first_target_tick
                    ),
                    "lastTargetTick": str(
                        item.last_target_tick
                    ),
                    "outcomes": _outcomes_document(
                        item.outcomes
                    ),
                }
                for item in statistics.command_types
            ],
            "rejectionCodes": [
                {
                    "rejectionCode": item.rejection_code,
                    "count": item.count,
                    "sessionCount": item.session_count,
                    "firstOrdinal": item.first_ordinal,
                    "lastOrdinal": item.last_ordinal,
                }
                for item in statistics.rejection_codes
            ],
            "ticks": [
                {
                    "targetTick": str(item.target_tick),
                    "sessionCount": item.session_count,
                    "commandTypes": list(
                        item.command_types
                    ),
                    "outcomes": _outcomes_document(
                        item.outcomes
                    ),
                }
                for item in statistics.ticks
            ],
        },
        "baseline": {
            "sampleCount": baseline.sample_count,
            "archiveDigests": list(
                baseline.archive_digests
            ),
            "policy": {
                "minimumSamples": (
                    baseline.policy.minimum_samples
                ),
                "modifiedZThreshold": _number(
                    baseline.policy.modified_z_threshold
                ),
            },
            "metrics": [
                {
                    "metric": item.metric,
                    "sampleCount": item.sample_count,
                    "minimum": _number(item.minimum),
                    "maximum": _number(item.maximum),
                    "mean": _number(item.mean),
                    "median": _number(item.median),
                    "medianAbsoluteDeviation": _number(
                        item.median_absolute_deviation
                    ),
                    "zeroMadTolerance": _number(
                        item.zero_mad_tolerance
                    ),
                }
                for item in baseline.metrics
            ],
        },
        "outlierAnalysis": {
            "evaluatedMetrics": list(
                report.outliers.evaluated_metrics
            ),
            "hasOutliers": report.outliers.has_outliers,
            "findings": [
                _finding_document(
                    finding,
                    reviews_by_metric.get(finding.metric),
                )
                for finding in report.outliers.findings
            ],
        },
        "reviewSummary": {
            "findingCount": len(
                report.outliers.findings
            ),
            "reviewedFindingCount": (
                report.reviewed_finding_count
            ),
            "unreviewedFindingCount": (
                report.unreviewed_finding_count
            ),
            "falsePositiveCount": (
                report.false_positive_count
            ),
        },
    }


def render_analysis_report_json(
    report: InvestigationAnalysisReport,
) -> str:
    return (
        json.dumps(
            analysis_report_document(report),
            indent=2,
            sort_keys=True,
            ensure_ascii=False,
        )
        + "\n"
    )


def write_analysis_report_json(
    report: InvestigationAnalysisReport,
    path: str | Path,
) -> None:
    Path(path).write_text(
        render_analysis_report_json(report),
        encoding="utf-8",
    )
