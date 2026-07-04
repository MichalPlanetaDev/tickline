#include "tickline/command/command_evidence.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace tickline::command {
namespace {

class EvidenceWriter final {
public:
    explicit EvidenceWriter(
        EncodedCommandEvidence& output) noexcept
        : output_{output}
    {
    }

    void append_byte(
        const std::byte value) noexcept
    {
        assert(offset_ < output_.size());

        output_[offset_] = value;
        ++offset_;
    }

    void append_u16(
        const std::uint16_t value) noexcept
    {
        append_byte(
            static_cast<std::byte>(
                (value >> 8U) & 0xffU));

        append_byte(
            static_cast<std::byte>(
                value & 0xffU));
    }

    void append_u64(
        const std::uint64_t value) noexcept
    {
        constexpr std::array<unsigned int, 8> shifts{
            56U,
            48U,
            40U,
            32U,
            24U,
            16U,
            8U,
            0U,
        };

        for (const auto shift : shifts) {
            append_byte(
                static_cast<std::byte>(
                    (value >> shift) &
                    0xffU));
        }
    }

    void append_i64(
        const std::int64_t value) noexcept
    {
        append_u64(
            std::bit_cast<std::uint64_t>(
                value));
    }

    void append_digest(
        const security::Sha256Digest& digest) noexcept
    {
        for (const auto byte : digest.bytes()) {
            append_byte(byte);
        }
    }

    [[nodiscard]] std::size_t
    size() const noexcept
    {
        return offset_;
    }

private:
    EncodedCommandEvidence& output_;
    std::size_t offset_{0};
};

class EvidenceReader final {
public:
    explicit EvidenceReader(
        const std::span<const std::byte> input) noexcept
        : input_{input}
    {
    }

    [[nodiscard]] std::byte read_byte() noexcept
    {
        assert(offset_ < input_.size());

        const auto value = input_[offset_];
        ++offset_;

        return value;
    }

    [[nodiscard]] std::uint16_t read_u16() noexcept
    {
        const auto first =
            std::to_integer<std::uint16_t>(
                read_byte());

        const auto second =
            std::to_integer<std::uint16_t>(
                read_byte());

        return static_cast<std::uint16_t>(
            (first << 8U) | second);
    }

    [[nodiscard]] std::uint64_t read_u64() noexcept
    {
        std::uint64_t value = 0;

        for (int index = 0; index < 8; ++index) {
            value =
                (value << 8U) |
                std::to_integer<std::uint64_t>(
                    read_byte());
        }

        return value;
    }

    [[nodiscard]] std::int64_t read_i64() noexcept
    {
        return std::bit_cast<std::int64_t>(
            read_u64());
    }

    [[nodiscard]] security::Sha256Digest
    read_digest() noexcept
    {
        security::Sha256Digest::Storage bytes{};

        for (auto& byte : bytes) {
            byte = read_byte();
        }

        return security::Sha256Digest{bytes};
    }

    [[nodiscard]] std::size_t
    size() const noexcept
    {
        return offset_;
    }

private:
    std::span<const std::byte> input_;
    std::size_t offset_{0};
};

[[noreturn]] void throw_decode_error(
    const CommandEvidenceDecodeErrorCode code,
    const std::string& message)
{
    throw CommandEvidenceDecodeError{
        code,
        message,
    };
}

[[nodiscard]] bool is_known_rejection_code(
    const CommandRejectionCode code) noexcept
{
    switch (code) {
    case CommandRejectionCode::none:
    case CommandRejectionCode::unsupported_schema_version:
    case CommandRejectionCode::unsupported_command_type:
    case CommandRejectionCode::invalid_client_id:
    case CommandRejectionCode::invalid_session_id:
    case CommandRejectionCode::client_identity_mismatch:
    case CommandRejectionCode::session_identity_mismatch:
    case CommandRejectionCode::sequence_zero:
    case CommandRejectionCode::stale_target_tick:
    case CommandRejectionCode::target_tick_too_far_future:
    case CommandRejectionCode::velocity_out_of_range:
    case CommandRejectionCode::duplicate_sequence:
    case CommandRejectionCode::sequence_regression:
    case CommandRejectionCode::unknown_entity:
    case CommandRejectionCode::entity_sequence_not_increasing:
    case CommandRejectionCode::target_tick_regression:
    case CommandRejectionCode::simulation_rejection_unknown:
        return true;
    }

    return false;
}

[[nodiscard]] bool is_known_queue_outcome(
    const CommandQueueOutcome outcome) noexcept
{
    switch (outcome) {
    case CommandQueueOutcome::not_attempted:
    case CommandQueueOutcome::accepted:
    case CommandQueueOutcome::stale_tick:
    case CommandQueueOutcome::unknown_entity:
    case CommandQueueOutcome::sequence_not_increasing:
    case CommandQueueOutcome::target_tick_regression:
    case CommandQueueOutcome::velocity_out_of_range:
    case CommandQueueOutcome::unknown:
        return true;
    }

    return false;
}

[[nodiscard]] std::optional<CommandRejectionCode>
expected_rejection_code(
    const CommandQueueOutcome outcome) noexcept
{
    switch (outcome) {
    case CommandQueueOutcome::not_attempted:
        return std::nullopt;

    case CommandQueueOutcome::accepted:
        return CommandRejectionCode::none;

    case CommandQueueOutcome::stale_tick:
        return CommandRejectionCode::stale_target_tick;

    case CommandQueueOutcome::unknown_entity:
        return CommandRejectionCode::unknown_entity;

    case CommandQueueOutcome::sequence_not_increasing:
        return CommandRejectionCode::
            entity_sequence_not_increasing;

    case CommandQueueOutcome::target_tick_regression:
        return CommandRejectionCode::
            target_tick_regression;

    case CommandQueueOutcome::velocity_out_of_range:
        return CommandRejectionCode::
            velocity_out_of_range;

    case CommandQueueOutcome::unknown:
        return CommandRejectionCode::
            simulation_rejection_unknown;
    }

    return std::nullopt;
}

void validate_record_invariants(
    const CommandEvidenceRecord& record)
{
    const auto accepted =
        record.entry.rejection_code ==
        CommandRejectionCode::none;

    if (accepted !=
        (record.entry.queue_outcome ==
         CommandQueueOutcome::accepted)) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                inconsistent_outcome,
            "evidence rejection code and queue outcome are inconsistent");
    }

    const auto expected =
        expected_rejection_code(
            record.entry.queue_outcome);

    if (expected.has_value() &&
        expected.value() !=
            record.entry.rejection_code) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                inconsistent_outcome,
            "evidence queue outcome does not match its rejection code");
    }

    if (accepted) {
        if (record.entry.session_sequence_after !=
                record.entry.envelope.sequence ||
            record.entry.session_sequence_after <=
                record.entry.session_sequence_before) {
            throw_decode_error(
                CommandEvidenceDecodeErrorCode::
                    inconsistent_state_transition,
                "accepted evidence has an invalid session sequence transition");
        }

        if (record.entry.pending_commands_before ==
            std::numeric_limits<std::uint64_t>::max()) {
            throw_decode_error(
                CommandEvidenceDecodeErrorCode::
                    inconsistent_state_transition,
                "accepted evidence pending-command count overflows");
        }

        if (record.entry.pending_commands_after !=
            record.entry.pending_commands_before + 1) {
            throw_decode_error(
                CommandEvidenceDecodeErrorCode::
                    inconsistent_state_transition,
                "accepted evidence has an invalid pending-command transition");
        }

        return;
    }

    if (record.entry.session_sequence_after !=
            record.entry.session_sequence_before ||
        record.entry.pending_commands_after !=
            record.entry.pending_commands_before) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                inconsistent_state_transition,
            "rejected evidence must not mutate session or queue state");
    }
}

}

CommandEvidenceDecodeError::CommandEvidenceDecodeError(
    const CommandEvidenceDecodeErrorCode code,
    std::string message)
    : std::runtime_error{std::move(message)},
      code_{code}
{
}

CommandEvidenceDecodeErrorCode
CommandEvidenceDecodeError::code() const noexcept
{
    return code_;
}

EncodedCommandEvidence encode_command_evidence(
    const CommandEvidenceRecord& record) noexcept
{
    constexpr std::array<std::byte, 4> magic{
        std::byte{'T'},
        std::byte{'L'},
        std::byte{'C'},
        std::byte{'E'},
    };

    EncodedCommandEvidence output{};
    EvidenceWriter writer{output};

    for (const auto byte : magic) {
        writer.append_byte(byte);
    }

    writer.append_u16(
        command_evidence_schema_version);

    writer.append_u16(0);

    writer.append_u64(record.ordinal);

    writer.append_digest(
        record.previous_record_digest);

    writer.append_u64(
        record.entry
            .world_fingerprint_before
            .value());

    writer.append_u64(
        record.entry.observed_tick);

    writer.append_u16(
        record.entry.envelope.schema_version);

    writer.append_u16(
        static_cast<std::uint16_t>(
            record.entry.envelope.type));

    writer.append_u64(
        record.entry.envelope
            .client_id
            .value());

    writer.append_u64(
        record.entry.envelope
            .session_id
            .value());

    writer.append_u64(
        record.entry.envelope.sequence);

    writer.append_u64(
        record.entry.envelope.target_tick);

    writer.append_u64(
        record.entry.envelope
            .payload
            .entity_id
            .value());

    writer.append_i64(
        record.entry.envelope
            .payload
            .velocity
            .x
            .count());

    writer.append_i64(
        record.entry.envelope
            .payload
            .velocity
            .y
            .count());

    writer.append_u64(
        record.entry.session_sequence_before);

    writer.append_u64(
        record.entry.session_sequence_after);

    writer.append_u64(
        record.entry.pending_commands_before);

    writer.append_u64(
        record.entry.pending_commands_after);

    writer.append_u16(
        static_cast<std::uint16_t>(
            record.entry.rejection_code));

    writer.append_u16(
        static_cast<std::uint16_t>(
            record.entry.queue_outcome));

    assert(writer.size() == output.size());

    return output;
}

CommandEvidenceRecord decode_command_evidence(
    const std::span<const std::byte> encoded)
{
    if (encoded.size() !=
        command_evidence_encoded_size) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                invalid_size,
            "command evidence record has an invalid encoded size");
    }

    constexpr std::array<std::byte, 4> expected_magic{
        std::byte{'T'},
        std::byte{'L'},
        std::byte{'C'},
        std::byte{'E'},
    };

    EvidenceReader reader{encoded};

    for (const auto expected : expected_magic) {
        if (reader.read_byte() != expected) {
            throw_decode_error(
                CommandEvidenceDecodeErrorCode::
                    invalid_magic,
                "command evidence record has an invalid magic value");
        }
    }

    const auto schema_version =
        reader.read_u16();

    if (schema_version !=
        command_evidence_schema_version) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                unsupported_schema_version,
            "command evidence schema version is unsupported");
    }

    if (reader.read_u16() != 0) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                nonzero_reserved_field,
            "command evidence reserved field must be zero");
    }

    const auto ordinal =
        reader.read_u64();

    const auto previous_digest =
        reader.read_digest();

    const auto world_fingerprint =
        simulation::StateFingerprint{
            reader.read_u64()};

    const auto observed_tick =
        reader.read_u64();

    const auto envelope_schema =
        reader.read_u16();

    const auto command_type =
        static_cast<CommandType>(
            reader.read_u16());

    const auto client_id =
        ClientId{reader.read_u64()};

    const auto session_id =
        SessionId{reader.read_u64()};

    const auto sequence =
        reader.read_u64();

    const auto target_tick =
        reader.read_u64();

    const auto entity_id_value =
        reader.read_u64();

    const auto velocity_x =
        reader.read_i64();

    const auto velocity_y =
        reader.read_i64();

    const auto session_sequence_before =
        reader.read_u64();

    const auto session_sequence_after =
        reader.read_u64();

    const auto pending_commands_before =
        reader.read_u64();

    const auto pending_commands_after =
        reader.read_u64();

    const auto rejection_code =
        static_cast<CommandRejectionCode>(
            reader.read_u16());

    if (!is_known_rejection_code(
            rejection_code)) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                unknown_rejection_code,
            "command evidence contains an unknown rejection code");
    }

    const auto queue_outcome =
        static_cast<CommandQueueOutcome>(
            reader.read_u16());

    if (!is_known_queue_outcome(
            queue_outcome)) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                unknown_queue_outcome,
            "command evidence contains an unknown queue outcome");
    }

    assert(reader.size() == encoded.size());

    std::optional<simulation::EntityId> entity_id;

    try {
        entity_id.emplace(entity_id_value);
    } catch (const std::invalid_argument&) {
        throw_decode_error(
            CommandEvidenceDecodeErrorCode::
                invalid_entity_id,
            "command evidence contains an invalid entity identifier");
    }

    CommandEvidenceRecord record{
        .ordinal = ordinal,
        .previous_record_digest =
            previous_digest,
        .entry =
            CommandEvidenceEntry{
                .world_fingerprint_before =
                    world_fingerprint,
                .observed_tick =
                    observed_tick,
                .envelope =
                    CommandEnvelope{
                        .schema_version =
                            envelope_schema,
                        .type =
                            command_type,
                        .client_id =
                            client_id,
                        .session_id =
                            session_id,
                        .sequence =
                            sequence,
                        .target_tick =
                            target_tick,
                        .payload =
                            SetVelocityPayload{
                                .entity_id =
                                    entity_id.value(),
                                .velocity =
                                    simulation::Velocity2{
                                        .x =
                                            simulation::
                                                MillimetersPerSecond{
                                                    velocity_x},
                                        .y =
                                            simulation::
                                                MillimetersPerSecond{
                                                    velocity_y},
                                    },
                            },
                    },
                .session_sequence_before =
                    session_sequence_before,
                .session_sequence_after =
                    session_sequence_after,
                .pending_commands_before =
                    pending_commands_before,
                .pending_commands_after =
                    pending_commands_after,
                .rejection_code =
                    rejection_code,
                .queue_outcome =
                    queue_outcome,
            },
    };

    validate_record_invariants(record);

    return record;
}

security::Sha256Digest digest_command_evidence(
    const CommandEvidenceRecord& record) noexcept
{
    const auto encoded =
        encode_command_evidence(record);

    return security::sha256(
        std::span<const std::byte>{
            encoded.data(),
            encoded.size(),
        });
}

bool verify_command_evidence_chain(
    const std::span<
        const CommandEvidenceRecord> records,
    const security::Sha256Digest expected_head) noexcept
{
    auto previous =
        security::Sha256Digest{};

    for (std::size_t index = 0;
         index < records.size();
         ++index) {
        if (!std::in_range<std::uint64_t>(
                index)) {
            return false;
        }

        const auto& record = records[index];

        if (record.ordinal !=
            static_cast<std::uint64_t>(
                index)) {
            return false;
        }

        if (record.previous_record_digest !=
            previous) {
            return false;
        }

        previous =
            digest_command_evidence(record);
    }

    return previous == expected_head;
}

std::span<const CommandEvidenceRecord>
CommandEvidenceLog::records() const noexcept
{
    return std::span<
        const CommandEvidenceRecord>{
            records_.data(),
            records_.size(),
        };
}

std::size_t CommandEvidenceLog::size() const noexcept
{
    return records_.size();
}

bool CommandEvidenceLog::empty() const noexcept
{
    return records_.empty();
}

security::Sha256Digest
CommandEvidenceLog::head_digest() const noexcept
{
    return head_digest_;
}

bool CommandEvidenceLog::verify() const noexcept
{
    return verify_command_evidence_chain(
        records(),
        head_digest_);
}

void CommandEvidenceLog::prepare_append()
{
    if (!std::in_range<std::uint64_t>(
            records_.size())) {
        throw std::overflow_error{
            "command evidence ordinal cannot be represented"};
    }

    if (records_.size() ==
        records_.max_size()) {
        throw std::length_error{
            "command evidence log reached its maximum size"};
    }

    records_.reserve(
        records_.size() + 1);
}

void CommandEvidenceLog::append(
    CommandEvidenceEntry entry) noexcept
{
    static_assert(
        std::is_nothrow_move_constructible_v<
            CommandEvidenceRecord>);

    assert(
        records_.size() <
        records_.capacity());

    assert(
        std::in_range<std::uint64_t>(
            records_.size()));

    CommandEvidenceRecord record{
        .ordinal =
            static_cast<std::uint64_t>(
                records_.size()),
        .previous_record_digest =
            head_digest_,
        .entry = std::move(entry),
    };

    const auto next_head =
        digest_command_evidence(record);

    records_.push_back(
        std::move(record));

    head_digest_ = next_head;
}

}
