"""Robust investigation baselines and explainable outlier detection."""

from __future__ import annotations

from dataclasses import dataclass
from statistics import fmean, median
from typing import Callable, Iterable

from .analytics import InvestigationStatistics

_MODIFIED_Z_SCALE = 0.6744897501960817


class InvestigationBaselineError(ValueError):
    """Raised when a baseline or candidate violates analytics constraints."""

    def __init__(self, code: str, message: str) -> None:
        self.code = code
        self.message = message
        super().__init__(f"{code}: {message}")


@dataclass(frozen=True, slots=True)
class OutlierPolicy:
    minimum_samples: int = 5
    modified_z_threshold: float = 3.5
    command_count_tolerance: float = 1.0
    rejection_rate_tolerance: float = 0.05
    session_count_tolerance: float = 1.0
    unique_target_ticks_tolerance: float = 1.0
    tick_span_tolerance: float = 1.0
    commands_per_tick_tolerance: float = 0.5

    def __post_init__(self) -> None:
        if self.minimum_samples < 3:
            raise ValueError("minimum_samples must be at least 3")

        positive_values = {
            "modified_z_threshold": self.modified_z_threshold,
            "command_count_tolerance": self.command_count_tolerance,
            "rejection_rate_tolerance": (
                self.rejection_rate_tolerance
            ),
            "session_count_tolerance": (
                self.session_count_tolerance
            ),
            "unique_target_ticks_tolerance": (
                self.unique_target_ticks_tolerance
            ),
            "tick_span_tolerance": self.tick_span_tolerance,
            "commands_per_tick_tolerance": (
                self.commands_per_tick_tolerance
            ),
        }

        for name, value in positive_values.items():
            if value <= 0:
                raise ValueError(f"{name} must be greater than zero")

    def tolerance_for(self, metric: str) -> float:
        tolerances = {
            "command_count": self.command_count_tolerance,
            "rejection_rate": self.rejection_rate_tolerance,
            "session_count": self.session_count_tolerance,
            "unique_target_ticks": (
                self.unique_target_ticks_tolerance
            ),
            "tick_span": self.tick_span_tolerance,
            "commands_per_tick": (
                self.commands_per_tick_tolerance
            ),
        }

        try:
            return tolerances[metric]
        except KeyError as error:
            raise KeyError(f"unknown baseline metric {metric!r}") from error


@dataclass(frozen=True, slots=True)
class MetricBaseline:
    metric: str
    sample_count: int
    minimum: float
    maximum: float
    mean: float
    median: float
    median_absolute_deviation: float
    zero_mad_tolerance: float


@dataclass(frozen=True, slots=True)
class InvestigationBaseline:
    sample_count: int
    archive_digests: tuple[str, ...]
    metrics: tuple[MetricBaseline, ...]
    policy: OutlierPolicy

    def metric(self, name: str) -> MetricBaseline:
        for item in self.metrics:
            if item.metric == name:
                return item

        raise KeyError(f"baseline does not contain metric {name!r}")


@dataclass(frozen=True, slots=True)
class OutlierFinding:
    metric: str
    direction: str
    observed: float
    baseline_median: float
    baseline_mad: float
    score: float
    threshold: float
    method: str
    explanation: str

    @property
    def normalized_severity(self) -> float:
        return self.score / self.threshold


@dataclass(frozen=True, slots=True)
class OutlierAnalysis:
    archive_digest: str
    baseline_sample_count: int
    evaluated_metrics: tuple[str, ...]
    findings: tuple[OutlierFinding, ...]

    @property
    def has_outliers(self) -> bool:
        return bool(self.findings)


MetricExtractor = Callable[[InvestigationStatistics], float]


def _commands_per_tick(
    statistics: InvestigationStatistics,
) -> float:
    if statistics.unique_target_ticks == 0:
        return 0.0

    return (
        statistics.outcomes.total
        / statistics.unique_target_ticks
    )


_METRIC_EXTRACTORS: tuple[tuple[str, MetricExtractor], ...] = (
    (
        "command_count",
        lambda statistics: float(statistics.outcomes.total),
    ),
    (
        "rejection_rate",
        lambda statistics: statistics.outcomes.rejection_rate,
    ),
    (
        "session_count",
        lambda statistics: float(statistics.session_count),
    ),
    (
        "unique_target_ticks",
        lambda statistics: float(
            statistics.unique_target_ticks
        ),
    ),
    (
        "tick_span",
        lambda statistics: float(statistics.tick_span),
    ),
    (
        "commands_per_tick",
        _commands_per_tick,
    ),
)


def _require_verified(
    statistics: InvestigationStatistics,
    role: str,
) -> None:
    if not statistics.replay_verified:
        raise InvestigationBaselineError(
            f"{role}.unverified",
            (
                f"investigation {statistics.archive_digest} "
                "does not have a verified replay result"
            ),
        )


def _build_metric_baseline(
    metric: str,
    values: list[float],
    policy: OutlierPolicy,
) -> MetricBaseline:
    center = float(median(values))
    deviations = [
        abs(value - center)
        for value in values
    ]

    return MetricBaseline(
        metric=metric,
        sample_count=len(values),
        minimum=min(values),
        maximum=max(values),
        mean=fmean(values),
        median=center,
        median_absolute_deviation=float(median(deviations)),
        zero_mad_tolerance=policy.tolerance_for(metric),
    )


def build_investigation_baseline(
    samples: Iterable[InvestigationStatistics],
    policy: OutlierPolicy | None = None,
) -> InvestigationBaseline:
    active_policy = policy or OutlierPolicy()
    sample_list = list(samples)

    if len(sample_list) < active_policy.minimum_samples:
        raise InvestigationBaselineError(
            "baseline.insufficient_samples",
            (
                f"expected at least {active_policy.minimum_samples} "
                f"verified investigations, got {len(sample_list)}"
            ),
        )

    archive_digests: set[str] = set()

    for statistics in sample_list:
        _require_verified(statistics, "baseline")

        if statistics.archive_digest in archive_digests:
            raise InvestigationBaselineError(
                "baseline.duplicate_archive",
                (
                    "baseline contains archive digest "
                    f"{statistics.archive_digest} more than once"
                ),
            )

        archive_digests.add(statistics.archive_digest)

    metrics = tuple(
        _build_metric_baseline(
            metric,
            [
                extractor(statistics)
                for statistics in sample_list
            ],
            active_policy,
        )
        for metric, extractor in _METRIC_EXTRACTORS
    )

    return InvestigationBaseline(
        sample_count=len(sample_list),
        archive_digests=tuple(sorted(archive_digests)),
        metrics=metrics,
        policy=active_policy,
    )


def _evaluate_metric(
    baseline: MetricBaseline,
    observed: float,
    policy: OutlierPolicy,
) -> OutlierFinding | None:
    delta = observed - baseline.median

    if delta == 0:
        return None

    direction = "high" if delta > 0 else "low"

    if baseline.median_absolute_deviation > 0:
        score = abs(
            _MODIFIED_Z_SCALE
            * delta
            / baseline.median_absolute_deviation
        )
        threshold = policy.modified_z_threshold
        method = "modified_z"

        if score < threshold:
            return None

        explanation = (
            f"{baseline.metric} is {direction}: observed "
            f"{observed:g}, baseline median "
            f"{baseline.median:g}, MAD "
            f"{baseline.median_absolute_deviation:g}, "
            f"modified z-score {score:.6g} "
            f"meets threshold {threshold:g}"
        )
    else:
        tolerance = baseline.zero_mad_tolerance
        score = abs(delta) / tolerance
        threshold = 1.0
        method = "zero_mad_tolerance"

        if score < threshold:
            return None

        explanation = (
            f"{baseline.metric} is {direction}: observed "
            f"{observed:g}, fixed baseline median "
            f"{baseline.median:g}, absolute deviation "
            f"{abs(delta):g} meets tolerance {tolerance:g}"
        )

    return OutlierFinding(
        metric=baseline.metric,
        direction=direction,
        observed=observed,
        baseline_median=baseline.median,
        baseline_mad=baseline.median_absolute_deviation,
        score=score,
        threshold=threshold,
        method=method,
        explanation=explanation,
    )


def evaluate_investigation_outliers(
    baseline: InvestigationBaseline,
    candidate: InvestigationStatistics,
) -> OutlierAnalysis:
    _require_verified(candidate, "candidate")

    if candidate.archive_digest in baseline.archive_digests:
        raise InvestigationBaselineError(
            "candidate.in_baseline",
            (
                "candidate archive digest is already included "
                "in the baseline"
            ),
        )

    metric_values = {
        metric: extractor(candidate)
        for metric, extractor in _METRIC_EXTRACTORS
    }

    findings = [
        finding
        for item in baseline.metrics
        if (
            finding := _evaluate_metric(
                item,
                metric_values[item.metric],
                baseline.policy,
            )
        )
        is not None
    ]

    findings.sort(
        key=lambda item: (
            -item.normalized_severity,
            item.metric,
        )
    )

    return OutlierAnalysis(
        archive_digest=candidate.archive_digest,
        baseline_sample_count=baseline.sample_count,
        evaluated_metrics=tuple(
            metric for metric, _ in _METRIC_EXTRACTORS
        ),
        findings=tuple(findings),
    )
