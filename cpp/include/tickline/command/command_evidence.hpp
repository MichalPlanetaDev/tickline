#pragma once

#include "tickline/command/command_envelope.hpp"
#include "tickline/command/command_validator.hpp"
#include "tickline/security/sha256.hpp"
#include "tickline/simulation/state_fingerprint.hpp"
#include "tickline/simulation/world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tickline::command {

inline constexpr std::uint16_t
command_evidence_schema_version = 2;

inline constexpr std::size_t
command_evidence_encoded_size = 160;

enum class CommandQueueOutcome : std::uint16_t {
    not_attempted = 0,
    accepted = 1,
    stale_tick = 2,
    unknown_entity = 3,
    sequence_not_increasing = 4,
    target_tick_regression = 5,
    velocity_out_of_range = 6,
    unknown = 65'535,
};

[[nodiscard]] constexpr CommandQueueOutcome
command_queue_outcome(
    const simulation::QueueCommandResult result) noexcept
{
    switch (result) {
    case simulation::QueueCommandResult::accepted:
        return CommandQueueOutcome::accepted;

    case simulation::QueueCommandResult::stale_tick:
        return CommandQueueOutcome::stale_tick;

    case simulation::QueueCommandResult::unknown_entity:
        return CommandQueueOutcome::unknown_entity;

    case simulation::QueueCommandResult::sequence_not_increasing:
        return CommandQueueOutcome::sequence_not_increasing;

    case simulation::QueueCommandResult::target_tick_regression:
        return CommandQueueOutcome::target_tick_regression;

    case simulation::QueueCommandResult::velocity_out_of_range:
        return CommandQueueOutcome::velocity_out_of_range;
    }

    return CommandQueueOutcome::unknown;
}

[[nodiscard]] constexpr std::string_view
command_queue_outcome_name(
    const CommandQueueOutcome outcome) noexcept
{
    switch (outcome) {
    case CommandQueueOutcome::not_attempted:
        return "not_attempted";

    case CommandQueueOutcome::accepted:
        return "accepted";

    case CommandQueueOutcome::stale_tick:
        return "stale_tick";

    case CommandQueueOutcome::unknown_entity:
        return "unknown_entity";

    case CommandQueueOutcome::sequence_not_increasing:
        return "sequence_not_increasing";

    case CommandQueueOutcome::target_tick_regression:
        return "target_tick_regression";

    case CommandQueueOutcome::velocity_out_of_range:
        return "velocity_out_of_range";

    case CommandQueueOutcome::unknown:
        return "unknown";
    }

    return "unknown";
}

struct CommandEvidenceEntry final {
    simulation::StateFingerprint world_fingerprint_before;
    std::uint64_t observed_tick;
    CommandEnvelope envelope;
    std::uint64_t session_sequence_before;
    std::uint64_t session_sequence_after;
    std::uint64_t pending_commands_before;
    std::uint64_t pending_commands_after;
    CommandRejectionCode rejection_code;
    CommandQueueOutcome queue_outcome;

    friend constexpr bool operator==(
        const CommandEvidenceEntry&,
        const CommandEvidenceEntry&) noexcept = default;
};

struct CommandEvidenceRecord final {
    std::uint64_t ordinal;
    security::Sha256Digest previous_record_digest;
    CommandEvidenceEntry entry;

    friend constexpr bool operator==(
        const CommandEvidenceRecord&,
        const CommandEvidenceRecord&) noexcept = default;
};

enum class CommandEvidenceDecodeErrorCode {
    invalid_size,
    invalid_magic,
    unsupported_schema_version,
    nonzero_reserved_field,
    invalid_entity_id,
    unknown_rejection_code,
    unknown_queue_outcome,
    inconsistent_outcome,
    inconsistent_state_transition,
};

class CommandEvidenceDecodeError final
    : public std::runtime_error {
public:
    CommandEvidenceDecodeError(
        CommandEvidenceDecodeErrorCode code,
        std::string message);

    [[nodiscard]] CommandEvidenceDecodeErrorCode
    code() const noexcept;

private:
    CommandEvidenceDecodeErrorCode code_;
};

using EncodedCommandEvidence =
    std::array<
        std::byte,
        command_evidence_encoded_size>;

[[nodiscard]] EncodedCommandEvidence
encode_command_evidence(
    const CommandEvidenceRecord& record) noexcept;

[[nodiscard]] CommandEvidenceRecord
decode_command_evidence(
    std::span<const std::byte> encoded);

[[nodiscard]] security::Sha256Digest
digest_command_evidence(
    const CommandEvidenceRecord& record) noexcept;

[[nodiscard]] bool verify_command_evidence_chain(
    std::span<const CommandEvidenceRecord> records,
    security::Sha256Digest expected_head) noexcept;

class AuthoritativeCommandPipeline;

class CommandEvidenceLog final {
public:
    [[nodiscard]] std::span<
        const CommandEvidenceRecord>
    records() const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] security::Sha256Digest
    head_digest() const noexcept;

    [[nodiscard]] bool verify() const noexcept;

private:
    void prepare_append();

    void append(CommandEvidenceEntry entry) noexcept;

    friend class AuthoritativeCommandPipeline;

    std::vector<CommandEvidenceRecord> records_;
    security::Sha256Digest head_digest_{};
};

}
