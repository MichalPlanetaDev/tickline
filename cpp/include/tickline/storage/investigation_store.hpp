#pragma once

#include "tickline/command/command_evidence.hpp"
#include "tickline/command/command_evidence_archive.hpp"
#include "tickline/command/command_evidence_replay.hpp"
#include "tickline/security/sha256.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace tickline::storage {

inline constexpr int investigation_schema_version = 1;

enum class InvestigationStoreErrorCode {
    open_failed,
    migration_failed,
    sqlite_failure,
    invalid_argument,
    archive_verification_failed,
    archive_not_found,
};

class InvestigationStoreError final : public std::runtime_error {
public:
    InvestigationStoreError(
        InvestigationStoreErrorCode code,
        std::string message);

    [[nodiscard]] InvestigationStoreErrorCode code() const noexcept;

private:
    InvestigationStoreErrorCode code_;
};

struct EvidenceArchiveImportResult final {
    std::int64_t archive_id;
    bool inserted;
    std::size_t record_count;
    security::Sha256Digest archive_digest;
    security::Sha256Digest chain_head;

    friend bool operator==(
        const EvidenceArchiveImportResult&,
        const EvidenceArchiveImportResult&) = default;
};

struct EvidenceArchiveSummary final {
    std::int64_t archive_id;
    std::string source;
    std::size_t record_count;
    security::Sha256Digest archive_digest;
    security::Sha256Digest chain_head;

    friend bool operator==(
        const EvidenceArchiveSummary&,
        const EvidenceArchiveSummary&) = default;
};

struct EvidenceSessionSummary final {
    command::ClientId client_id;
    command::SessionId session_id;
    std::uint64_t first_ordinal;
    std::uint64_t last_ordinal;
    std::uint64_t first_observed_tick;
    std::uint64_t last_observed_tick;
    std::size_t accepted_count;
    std::size_t rejected_count;

    friend bool operator==(
        const EvidenceSessionSummary&,
        const EvidenceSessionSummary&) = default;
};

struct EvidenceQuery final {
    std::optional<command::ClientId> client_id;
    std::optional<command::SessionId> session_id;
    std::optional<command::CommandRejectionCode> rejection_code;
    std::optional<command::CommandQueueOutcome> queue_outcome;
    std::optional<std::uint64_t> minimum_observed_tick;
    std::optional<std::uint64_t> maximum_observed_tick;
    std::optional<std::uint64_t> after_ordinal;
    std::size_t limit{100};
};

struct StoredEvidenceRecord final {
    std::int64_t archive_id;
    security::Sha256Digest record_digest;
    command::CommandEvidenceRecord record;

    friend bool operator==(
        const StoredEvidenceRecord&,
        const StoredEvidenceRecord&) = default;
};

class InvestigationStore final {
public:
    explicit InvestigationStore(const std::filesystem::path& path);
    ~InvestigationStore();

    InvestigationStore(const InvestigationStore&) = delete;
    InvestigationStore& operator=(const InvestigationStore&) = delete;

    InvestigationStore(InvestigationStore&&) noexcept;
    InvestigationStore& operator=(InvestigationStore&&) noexcept;

    [[nodiscard]] int schema_version() const;

    [[nodiscard]] EvidenceArchiveImportResult import_archive(
        std::span<const std::byte> encoded_archive,
        security::Sha256Digest trusted_head,
        std::string source);

    void record_replay_result(
        std::int64_t archive_id,
        const command::CommandEvidenceReplayResult& result);

    [[nodiscard]] std::vector<EvidenceArchiveSummary> list_archives(
        std::size_t limit = 100,
        std::optional<std::int64_t> before_archive_id = std::nullopt) const;

    [[nodiscard]] std::vector<StoredEvidenceRecord> query_evidence(
        std::int64_t archive_id,
        const EvidenceQuery& query = {}) const;

    [[nodiscard]] std::vector<EvidenceSessionSummary> list_sessions(
        std::int64_t archive_id) const;

    [[nodiscard]] std::optional<command::CommandEvidenceReplayResult>
    replay_result(std::int64_t archive_id) const;

    [[nodiscard]] std::size_t archive_count() const;
    [[nodiscard]] std::size_t evidence_record_count() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
