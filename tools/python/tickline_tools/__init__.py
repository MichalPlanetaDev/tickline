"""Python support tools for Tickline analysis and automation."""

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
    "InvestigationBundle",
    "InvestigationBundleValidationError",
    "InvestigationEvidenceRecord",
    "InvestigationSession",
    "ReplaySummary",
    "__version__",
    "load_investigation_bundle",
    "parse_investigation_bundle_json",
]

__version__ = "0.7.0"
