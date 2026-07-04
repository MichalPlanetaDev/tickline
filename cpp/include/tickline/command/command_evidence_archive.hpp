#pragma once

#include "tickline/command/command_evidence.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace tickline::command {

inline constexpr std::uint16_t
command_evidence_archive_schema_version = 1;

inline constexpr std::size_t
command_evidence_archive_header_size = 56;

enum class CommandEvidenceArchiveErrorCode {
    truncated_header,
    invalid_magic,
    unsupported_archive_schema,
    unsupported_record_schema,
    invalid_record_size,
    nonzero_reserved_field,
    record_count_overflow,
    truncated_records,
    trailing_data,
    chain_verification_failed,
    trusted_head_mismatch,
    io_error,
};

class CommandEvidenceArchiveError final
    : public std::runtime_error {
public:
    CommandEvidenceArchiveError(
        CommandEvidenceArchiveErrorCode code,
        std::string message);

    [[nodiscard]] CommandEvidenceArchiveErrorCode
    code() const noexcept;

private:
    CommandEvidenceArchiveErrorCode code_;
};

struct CommandEvidenceArchive final {
    std::vector<CommandEvidenceRecord> records;
    security::Sha256Digest head_digest;

    friend bool operator==(
        const CommandEvidenceArchive&,
        const CommandEvidenceArchive&) = default;
};

[[nodiscard]] std::vector<std::byte>
encode_command_evidence_archive(
    std::span<const CommandEvidenceRecord> records,
    security::Sha256Digest head_digest);

[[nodiscard]] CommandEvidenceArchive
decode_command_evidence_archive(
    std::span<const std::byte> encoded,
    security::Sha256Digest trusted_head);

void write_command_evidence_archive(
    const std::filesystem::path& path,
    std::span<const CommandEvidenceRecord> records,
    security::Sha256Digest head_digest);

void write_command_evidence_archive(
    const std::filesystem::path& path,
    const CommandEvidenceLog& evidence);

[[nodiscard]] CommandEvidenceArchive
read_command_evidence_archive(
    const std::filesystem::path& path,
    security::Sha256Digest trusted_head);

}
