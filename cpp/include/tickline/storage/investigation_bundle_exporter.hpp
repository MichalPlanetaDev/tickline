#pragma once

#include "tickline/storage/investigation_store.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace tickline::storage {

inline constexpr int investigation_bundle_schema_version = 1;

enum class InvestigationBundleExportErrorCode {
    invalid_archive_id,
    archive_not_found,
    replay_result_missing,
    evidence_count_mismatch,
    evidence_chain_mismatch,
    session_summary_mismatch,
    json_integer_out_of_range,
};

class InvestigationBundleExportError final : public std::runtime_error {
public:
    InvestigationBundleExportError(
        InvestigationBundleExportErrorCode code,
        std::string message);

    [[nodiscard]] InvestigationBundleExportErrorCode code() const noexcept;

private:
    InvestigationBundleExportErrorCode code_;
};

[[nodiscard]] std::string export_investigation_bundle_json(
    const InvestigationStore& store,
    std::int64_t archive_id);

}
