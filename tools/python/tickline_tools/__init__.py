"""Python support tools for Tickline analysis and automation."""

from .analytics import (
    CommandTypeStatistics,
    InvestigationStatistics,
    OutcomeStatistics,
    RejectionCodeStatistics,
    SessionStatistics,
    TickStatistics,
    analyze_investigation,
)
from .investigation_bundle import (
    SUPPORTED_SCHEMA_VERSION,
    InvestigationBundle,
    InvestigationBundleValidationError,
    InvestigationEvidenceRecord,
    InvestigationSession,
    ReplaySummary,
    load_investigation_bundle,
    parse_investigation_bundle_json,
)

__all__ = [
    "SUPPORTED_SCHEMA_VERSION",
    "CommandTypeStatistics",
    "InvestigationBundle",
    "InvestigationBundleValidationError",
    "InvestigationEvidenceRecord",
    "InvestigationSession",
    "InvestigationStatistics",
    "OutcomeStatistics",
    "RejectionCodeStatistics",
    "ReplaySummary",
    "SessionStatistics",
    "TickStatistics",
    "__version__",
    "analyze_investigation",
    "load_investigation_bundle",
    "parse_investigation_bundle_json",
]

__version__ = "0.7.0"
