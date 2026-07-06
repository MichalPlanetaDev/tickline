"""Deterministic descriptive statistics for investigation bundles."""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime

from .investigation_bundle import (
    InvestigationBundle,
    InvestigationEvidenceRecord,
)


@dataclass(frozen=True, slots=True)
class OutcomeStatistics:
    accepted: int
    rejected: int

    @property
    def total(self) -> int:
        return self.accepted + self.rejected

    @property
    def acceptance_rate(self) -> float:
        if self.total == 0:
            return 0.0
        return self.accepted / self.total

    @property
    def rejection_rate(self) -> float:
        if self.total == 0:
            return 0.0
        return self.rejected / self.total


@dataclass(frozen=True, slots=True)
class SessionStatistics:
    client_id: int
    session_id: int
    last_committed_sequence: int
    first_target_tick: int
    last_target_tick: int
    tick_span: int
    unique_target_ticks: int
    first_ordinal: int | None
    last_ordinal: int | None
    command_types: tuple[str, ...]
    outcomes: OutcomeStatistics

    @property
    def key(self) -> tuple[int, int]:
        return self.client_id, self.session_id


@dataclass(frozen=True, slots=True)
class CommandTypeStatistics:
    command_type: str
    outcomes: OutcomeStatistics
    session_count: int
    first_target_tick: int
    last_target_tick: int


@dataclass(frozen=True, slots=True)
class RejectionCodeStatistics:
    rejection_code: str
    count: int
    session_count: int
    first_ordinal: int
    last_ordinal: int


@dataclass(frozen=True, slots=True)
class TickStatistics:
    target_tick: int
    outcomes: OutcomeStatistics
    session_count: int
    command_types: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class InvestigationStatistics:
    schema_version: int
    archive_digest: str
    imported_at_utc: datetime
    replay_verified: bool
    final_tick: int
    final_world_fingerprint: int
    outcomes: OutcomeStatistics
    session_count: int
    unique_clients: int
    unique_target_ticks: int
    first_target_tick: int | None
    last_target_tick: int | None
    tick_span: int
    busiest_ticks: tuple[int, ...]
    sessions: tuple[SessionStatistics, ...]
    command_types: tuple[CommandTypeStatistics, ...]
    rejection_codes: tuple[RejectionCodeStatistics, ...]
    ticks: tuple[TickStatistics, ...]


def _outcomes(
    records: list[InvestigationEvidenceRecord],
) -> OutcomeStatistics:
    accepted = sum(record.is_accepted for record in records)
    rejected = sum(record.is_rejected for record in records)

    return OutcomeStatistics(
        accepted=accepted,
        rejected=rejected,
    )


def _tick_span(
    first_target_tick: int | None,
    last_target_tick: int | None,
) -> int:
    if first_target_tick is None or last_target_tick is None:
        return 0

    return last_target_tick - first_target_tick + 1


def analyze_investigation(
    bundle: InvestigationBundle,
) -> InvestigationStatistics:
    records = list(bundle.evidence)

    records_by_session: dict[
        tuple[int, int],
        list[InvestigationEvidenceRecord],
    ] = defaultdict(list)
    records_by_command_type: dict[
        str,
        list[InvestigationEvidenceRecord],
    ] = defaultdict(list)
    records_by_rejection_code: dict[
        str,
        list[InvestigationEvidenceRecord],
    ] = defaultdict(list)
    records_by_tick: dict[
        int,
        list[InvestigationEvidenceRecord],
    ] = defaultdict(list)

    for record in records:
        records_by_session[record.key].append(record)
        records_by_command_type[record.command_type].append(record)
        records_by_tick[record.target_tick].append(record)

        if record.is_rejected:
            records_by_rejection_code[
                record.rejection_code
            ].append(record)

    session_statistics: list[SessionStatistics] = []

    for session in sorted(
        bundle.sessions,
        key=lambda item: item.key,
    ):
        session_records = records_by_session[session.key]
        target_ticks = {
            record.target_tick for record in session_records
        }

        session_statistics.append(
            SessionStatistics(
                client_id=session.client_id,
                session_id=session.session_id,
                last_committed_sequence=(
                    session.last_committed_sequence
                ),
                first_target_tick=session.first_target_tick,
                last_target_tick=session.last_target_tick,
                tick_span=_tick_span(
                    session.first_target_tick
                    if session_records
                    else None,
                    session.last_target_tick
                    if session_records
                    else None,
                ),
                unique_target_ticks=len(target_ticks),
                first_ordinal=(
                    session_records[0].ordinal
                    if session_records
                    else None
                ),
                last_ordinal=(
                    session_records[-1].ordinal
                    if session_records
                    else None
                ),
                command_types=tuple(
                    sorted(
                        {
                            record.command_type
                            for record in session_records
                        }
                    )
                ),
                outcomes=_outcomes(session_records),
            )
        )

    command_type_statistics: list[CommandTypeStatistics] = []

    for command_type, command_records in (
        records_by_command_type.items()
    ):
        target_ticks = [
            record.target_tick for record in command_records
        ]

        command_type_statistics.append(
            CommandTypeStatistics(
                command_type=command_type,
                outcomes=_outcomes(command_records),
                session_count=len(
                    {record.key for record in command_records}
                ),
                first_target_tick=min(target_ticks),
                last_target_tick=max(target_ticks),
            )
        )

    command_type_statistics.sort(
        key=lambda item: (
            -item.outcomes.total,
            item.command_type,
        )
    )

    rejection_statistics: list[RejectionCodeStatistics] = []

    for rejection_code, rejected_records in (
        records_by_rejection_code.items()
    ):
        rejection_statistics.append(
            RejectionCodeStatistics(
                rejection_code=rejection_code,
                count=len(rejected_records),
                session_count=len(
                    {record.key for record in rejected_records}
                ),
                first_ordinal=min(
                    record.ordinal for record in rejected_records
                ),
                last_ordinal=max(
                    record.ordinal for record in rejected_records
                ),
            )
        )

    rejection_statistics.sort(
        key=lambda item: (
            -item.count,
            item.rejection_code,
        )
    )

    tick_statistics: list[TickStatistics] = []

    for target_tick in sorted(records_by_tick):
        tick_records = records_by_tick[target_tick]

        tick_statistics.append(
            TickStatistics(
                target_tick=target_tick,
                outcomes=_outcomes(tick_records),
                session_count=len(
                    {record.key for record in tick_records}
                ),
                command_types=tuple(
                    sorted(
                        {
                            record.command_type
                            for record in tick_records
                        }
                    )
                ),
            )
        )

    first_target_tick = (
        tick_statistics[0].target_tick
        if tick_statistics
        else None
    )
    last_target_tick = (
        tick_statistics[-1].target_tick
        if tick_statistics
        else None
    )

    busiest_count = max(
        (
            tick.outcomes.total
            for tick in tick_statistics
        ),
        default=0,
    )
    busiest_ticks = tuple(
        tick.target_tick
        for tick in tick_statistics
        if tick.outcomes.total == busiest_count
    )

    return InvestigationStatistics(
        schema_version=bundle.schema_version,
        archive_digest=bundle.archive_digest,
        imported_at_utc=bundle.imported_at_utc,
        replay_verified=bundle.replay.verified,
        final_tick=bundle.replay.final_tick,
        final_world_fingerprint=(
            bundle.replay.final_world_fingerprint
        ),
        outcomes=_outcomes(records),
        session_count=len(bundle.sessions),
        unique_clients=len(
            {session.client_id for session in bundle.sessions}
        ),
        unique_target_ticks=len(tick_statistics),
        first_target_tick=first_target_tick,
        last_target_tick=last_target_tick,
        tick_span=_tick_span(
            first_target_tick,
            last_target_tick,
        ),
        busiest_ticks=busiest_ticks,
        sessions=tuple(session_statistics),
        command_types=tuple(command_type_statistics),
        rejection_codes=tuple(rejection_statistics),
        ticks=tuple(tick_statistics),
    )
