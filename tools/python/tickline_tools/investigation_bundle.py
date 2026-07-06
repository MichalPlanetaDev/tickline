"""Validated investigation-bundle input for Tickline analytics."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

SUPPORTED_SCHEMA_VERSION = 1

_MAX_JSON_INTEGER = 2_147_483_647
_MAX_UINT64 = 18_446_744_073_709_551_615
_UINT64_PATTERN = re.compile(r"(?:0|[1-9][0-9]*)\Z")
_DIGEST_PATTERN = re.compile(r"[0-9a-f]{64}\Z")


class InvestigationBundleValidationError(ValueError):
    """Raised when an investigation bundle violates its input contract."""

    def __init__(self, code: str, path: str, message: str) -> None:
        self.code = code
        self.path = path
        self.message = message
        super().__init__(f"{code} at {path}: {message}")


@dataclass(frozen=True, slots=True)
class InvestigationSession:
    client_id: int
    session_id: int
    last_committed_sequence: int
    first_target_tick: int
    last_target_tick: int
    accepted_commands: int
    rejected_commands: int

    @property
    def key(self) -> tuple[int, int]:
        return self.client_id, self.session_id

    @property
    def total_commands(self) -> int:
        return self.accepted_commands + self.rejected_commands


@dataclass(frozen=True, slots=True)
class InvestigationEvidenceRecord:
    ordinal: int
    client_id: int
    session_id: int
    session_sequence: int
    target_tick: int
    command_type: str
    outcome: str
    rejection_code: str
    previous_digest: str
    record_digest: str

    @property
    def key(self) -> tuple[int, int]:
        return self.client_id, self.session_id

    @property
    def is_accepted(self) -> bool:
        return self.outcome == "accepted"

    @property
    def is_rejected(self) -> bool:
        return self.outcome == "rejected"


@dataclass(frozen=True, slots=True)
class ReplaySummary:
    verified: bool
    final_tick: int
    final_world_fingerprint: int
    accepted_commands: int
    rejected_commands: int

    @property
    def total_commands(self) -> int:
        return self.accepted_commands + self.rejected_commands


@dataclass(frozen=True, slots=True)
class InvestigationBundle:
    schema_version: int
    archive_digest: str
    initial_head_digest: str
    trusted_head_digest: str
    imported_at_utc: datetime
    sessions: tuple[InvestigationSession, ...]
    evidence: tuple[InvestigationEvidenceRecord, ...]
    replay: ReplaySummary

    @property
    def total_commands(self) -> int:
        return len(self.evidence)


class _DuplicateJsonKeyError(ValueError):
    pass


def _fail(code: str, path: str, message: str) -> None:
    raise InvestigationBundleValidationError(code, path, message)


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}

    for key, value in pairs:
        if key in result:
            raise _DuplicateJsonKeyError(
                f"duplicate JSON object key {key!r}"
            )
        result[key] = value

    return result


def _reject_non_finite_number(value: str) -> None:
    raise _DuplicateJsonKeyError(f"non-finite JSON number {value!r}")


def _decode_json(text: str) -> Any:
    try:
        return json.loads(
            text,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_non_finite_number,
        )
    except (json.JSONDecodeError, _DuplicateJsonKeyError) as error:
        raise InvestigationBundleValidationError(
            "json.invalid",
            "$",
            str(error),
        ) from error


def _object(
    value: Any,
    path: str,
    expected_fields: set[str],
) -> dict[str, Any]:
    if type(value) is not dict:
        _fail("type.object", path, "expected a JSON object")

    missing = sorted(expected_fields - value.keys())
    if missing:
        name = missing[0]
        _fail(
            "field.missing",
            f"{path}.{name}",
            "required field is missing",
        )

    unexpected = sorted(value.keys() - expected_fields)
    if unexpected:
        name = unexpected[0]
        _fail(
            "field.unexpected",
            f"{path}.{name}",
            "field is not defined by schema version 1",
        )

    return value


def _array(value: Any, path: str) -> list[Any]:
    if type(value) is not list:
        _fail("type.array", path, "expected a JSON array")
    return value


def _string(value: Any, path: str) -> str:
    if type(value) is not str:
        _fail("type.string", path, "expected a JSON string")
    if not value:
        _fail("value.empty", path, "string must not be empty")
    return value


def _boolean(value: Any, path: str) -> bool:
    if type(value) is not bool:
        _fail("type.boolean", path, "expected a JSON boolean")
    return value


def _nonnegative_integer(value: Any, path: str) -> int:
    if type(value) is not int:
        _fail("type.integer", path, "expected a JSON integer")

    if not 0 <= value <= _MAX_JSON_INTEGER:
        _fail(
            "integer.range",
            path,
            f"expected a value from 0 through {_MAX_JSON_INTEGER}",
        )

    return value


def _uint64_string(value: Any, path: str) -> int:
    text = _string(value, path)

    if _UINT64_PATTERN.fullmatch(text) is None:
        _fail(
            "uint64.format",
            path,
            "expected a canonical unsigned decimal string",
        )

    number = int(text)

    if number > _MAX_UINT64:
        _fail(
            "uint64.range",
            path,
            f"value exceeds {_MAX_UINT64}",
        )

    return number


def _digest(value: Any, path: str) -> str:
    text = _string(value, path)

    if _DIGEST_PATTERN.fullmatch(text) is None:
        _fail(
            "digest.format",
            path,
            "expected exactly 64 lowercase hexadecimal characters",
        )

    return text


def _utc_timestamp(value: Any, path: str) -> datetime:
    text = _string(value, path)

    try:
        parsed = datetime.strptime(text, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError:
        _fail(
            "timestamp.format",
            path,
            "expected UTC timestamp in YYYY-MM-DDTHH:MM:SSZ format",
        )

    return parsed.replace(tzinfo=timezone.utc)


def _parse_session(value: Any, index: int) -> InvestigationSession:
    path = f"$.sessions[{index}]"
    fields = {
        "clientId",
        "sessionId",
        "lastCommittedSequence",
        "firstTargetTick",
        "lastTargetTick",
        "acceptedCommands",
        "rejectedCommands",
    }
    data = _object(value, path, fields)

    return InvestigationSession(
        client_id=_uint64_string(
            data["clientId"],
            f"{path}.clientId",
        ),
        session_id=_uint64_string(
            data["sessionId"],
            f"{path}.sessionId",
        ),
        last_committed_sequence=_uint64_string(
            data["lastCommittedSequence"],
            f"{path}.lastCommittedSequence",
        ),
        first_target_tick=_uint64_string(
            data["firstTargetTick"],
            f"{path}.firstTargetTick",
        ),
        last_target_tick=_uint64_string(
            data["lastTargetTick"],
            f"{path}.lastTargetTick",
        ),
        accepted_commands=_nonnegative_integer(
            data["acceptedCommands"],
            f"{path}.acceptedCommands",
        ),
        rejected_commands=_nonnegative_integer(
            data["rejectedCommands"],
            f"{path}.rejectedCommands",
        ),
    )


def _parse_evidence(
    value: Any,
    index: int,
) -> InvestigationEvidenceRecord:
    path = f"$.evidence[{index}]"
    fields = {
        "ordinal",
        "clientId",
        "sessionId",
        "sessionSequence",
        "targetTick",
        "commandType",
        "outcome",
        "rejectionCode",
        "previousDigest",
        "recordDigest",
    }
    data = _object(value, path, fields)

    return InvestigationEvidenceRecord(
        ordinal=_nonnegative_integer(
            data["ordinal"],
            f"{path}.ordinal",
        ),
        client_id=_uint64_string(
            data["clientId"],
            f"{path}.clientId",
        ),
        session_id=_uint64_string(
            data["sessionId"],
            f"{path}.sessionId",
        ),
        session_sequence=_uint64_string(
            data["sessionSequence"],
            f"{path}.sessionSequence",
        ),
        target_tick=_uint64_string(
            data["targetTick"],
            f"{path}.targetTick",
        ),
        command_type=_string(
            data["commandType"],
            f"{path}.commandType",
        ),
        outcome=_string(
            data["outcome"],
            f"{path}.outcome",
        ),
        rejection_code=_string(
            data["rejectionCode"],
            f"{path}.rejectionCode",
        ),
        previous_digest=_digest(
            data["previousDigest"],
            f"{path}.previousDigest",
        ),
        record_digest=_digest(
            data["recordDigest"],
            f"{path}.recordDigest",
        ),
    )


def _parse_replay(value: Any) -> ReplaySummary:
    path = "$.replay"
    fields = {
        "verified",
        "finalTick",
        "finalWorldFingerprint",
        "acceptedCommands",
        "rejectedCommands",
    }
    data = _object(value, path, fields)

    return ReplaySummary(
        verified=_boolean(
            data["verified"],
            f"{path}.verified",
        ),
        final_tick=_uint64_string(
            data["finalTick"],
            f"{path}.finalTick",
        ),
        final_world_fingerprint=_uint64_string(
            data["finalWorldFingerprint"],
            f"{path}.finalWorldFingerprint",
        ),
        accepted_commands=_nonnegative_integer(
            data["acceptedCommands"],
            f"{path}.acceptedCommands",
        ),
        rejected_commands=_nonnegative_integer(
            data["rejectedCommands"],
            f"{path}.rejectedCommands",
        ),
    )


def _validate_consistency(bundle: InvestigationBundle) -> None:
    sessions: dict[tuple[int, int], InvestigationSession] = {}
    session_indexes: dict[tuple[int, int], int] = {}
    records_by_session: dict[
        tuple[int, int],
        list[InvestigationEvidenceRecord],
    ] = {}

    for index, session in enumerate(bundle.sessions):
        path = f"$.sessions[{index}]"

        if session.key in sessions:
            _fail(
                "session.duplicate",
                path,
                "clientId and sessionId pair must be unique",
            )

        if session.first_target_tick > session.last_target_tick:
            _fail(
                "session.tick_range",
                path,
                "firstTargetTick must not exceed lastTargetTick",
            )

        sessions[session.key] = session
        session_indexes[session.key] = index
        records_by_session[session.key] = []

    expected_previous_digest = bundle.initial_head_digest
    record_digests: set[str] = set()

    for index, record in enumerate(bundle.evidence):
        path = f"$.evidence[{index}]"

        if record.ordinal != index:
            _fail(
                "evidence.ordinal",
                f"{path}.ordinal",
                f"expected contiguous ordinal {index}",
            )

        if record.key not in sessions:
            _fail(
                "evidence.unknown_session",
                path,
                "record references a session absent from sessions",
            )

        if record.previous_digest != expected_previous_digest:
            _fail(
                "evidence.chain_discontinuity",
                f"{path}.previousDigest",
                f"expected {expected_previous_digest}",
            )

        if record.record_digest in record_digests:
            _fail(
                "evidence.duplicate_digest",
                f"{path}.recordDigest",
                "record digest must be unique",
            )

        if record.outcome not in {"accepted", "rejected"}:
            _fail(
                "evidence.outcome",
                f"{path}.outcome",
                "expected accepted or rejected",
            )

        if record.is_accepted and record.rejection_code != "none":
            _fail(
                "evidence.accepted_rejection_code",
                f"{path}.rejectionCode",
                "accepted evidence must use rejectionCode none",
            )

        if record.is_rejected and record.rejection_code == "none":
            _fail(
                "evidence.rejected_rejection_code",
                f"{path}.rejectionCode",
                "rejected evidence must provide a rejection code",
            )

        records_by_session[record.key].append(record)
        record_digests.add(record.record_digest)
        expected_previous_digest = record.record_digest

    if expected_previous_digest != bundle.trusted_head_digest:
        _fail(
            "evidence.trusted_head_mismatch",
            "$.trustedHeadDigest",
            f"expected {expected_previous_digest}",
        )

    accepted_total = sum(
        record.is_accepted for record in bundle.evidence
    )
    rejected_total = sum(
        record.is_rejected for record in bundle.evidence
    )

    if bundle.replay.accepted_commands != accepted_total:
        _fail(
            "replay.accepted_count",
            "$.replay.acceptedCommands",
            f"expected {accepted_total}",
        )

    if bundle.replay.rejected_commands != rejected_total:
        _fail(
            "replay.rejected_count",
            "$.replay.rejectedCommands",
            f"expected {rejected_total}",
        )

    if bundle.evidence:
        last_target_tick = max(
            record.target_tick for record in bundle.evidence
        )

        if bundle.replay.final_tick < last_target_tick:
            _fail(
                "replay.final_tick",
                "$.replay.finalTick",
                f"must be at least {last_target_tick}",
            )

    for key, session in sessions.items():
        index = session_indexes[key]
        path = f"$.sessions[{index}]"
        records = records_by_session[key]
        accepted = [
            record for record in records if record.is_accepted
        ]
        rejected = [
            record for record in records if record.is_rejected
        ]

        if session.accepted_commands != len(accepted):
            _fail(
                "session.accepted_count",
                f"{path}.acceptedCommands",
                f"expected {len(accepted)}",
            )

        if session.rejected_commands != len(rejected):
            _fail(
                "session.rejected_count",
                f"{path}.rejectedCommands",
                f"expected {len(rejected)}",
            )

        expected_sequence = max(
            (record.session_sequence for record in accepted),
            default=0,
        )

        if session.last_committed_sequence != expected_sequence:
            _fail(
                "session.last_committed_sequence",
                f"{path}.lastCommittedSequence",
                f"expected {expected_sequence}",
            )

        expected_first_tick = min(
            (record.target_tick for record in records),
            default=0,
        )
        expected_last_tick = max(
            (record.target_tick for record in records),
            default=0,
        )

        if session.first_target_tick != expected_first_tick:
            _fail(
                "session.first_target_tick",
                f"{path}.firstTargetTick",
                f"expected {expected_first_tick}",
            )

        if session.last_target_tick != expected_last_tick:
            _fail(
                "session.last_target_tick",
                f"{path}.lastTargetTick",
                f"expected {expected_last_tick}",
            )


def parse_investigation_bundle_json(
    text: str,
) -> InvestigationBundle:
    fields = {
        "schemaVersion",
        "archiveDigest",
        "initialHeadDigest",
        "trustedHeadDigest",
        "importedAtUtc",
        "sessions",
        "evidence",
        "replay",
    }
    data = _object(_decode_json(text), "$", fields)

    schema_version = _nonnegative_integer(
        data["schemaVersion"],
        "$.schemaVersion",
    )

    if schema_version != SUPPORTED_SCHEMA_VERSION:
        _fail(
            "schema.unsupported",
            "$.schemaVersion",
            f"supported schema version is {SUPPORTED_SCHEMA_VERSION}",
        )

    sessions = tuple(
        _parse_session(value, index)
        for index, value in enumerate(
            _array(data["sessions"], "$.sessions")
        )
    )
    evidence = tuple(
        _parse_evidence(value, index)
        for index, value in enumerate(
            _array(data["evidence"], "$.evidence")
        )
    )

    bundle = InvestigationBundle(
        schema_version=schema_version,
        archive_digest=_digest(
            data["archiveDigest"],
            "$.archiveDigest",
        ),
        initial_head_digest=_digest(
            data["initialHeadDigest"],
            "$.initialHeadDigest",
        ),
        trusted_head_digest=_digest(
            data["trustedHeadDigest"],
            "$.trustedHeadDigest",
        ),
        imported_at_utc=_utc_timestamp(
            data["importedAtUtc"],
            "$.importedAtUtc",
        ),
        sessions=sessions,
        evidence=evidence,
        replay=_parse_replay(data["replay"]),
    )

    _validate_consistency(bundle)
    return bundle


def load_investigation_bundle(
    path: str | Path,
) -> InvestigationBundle:
    return parse_investigation_bundle_json(
        Path(path).read_text(encoding="utf-8")
    )
