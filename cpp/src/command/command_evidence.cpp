#include "tickline/command/command_evidence.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
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

    void append_byte(const std::byte value) noexcept
    {
        assert(offset_ < output_.size());
        output_[offset_] = value;
        ++offset_;
    }

    void append_u16(const std::uint16_t value) noexcept
    {
        append_byte(
            static_cast<std::byte>(
                (value >> 8U) & 0xffU));

        append_byte(
            static_cast<std::byte>(
                value & 0xffU));
    }

    void append_u64(const std::uint64_t value) noexcept
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
                    (value >> shift) & 0xffU));
        }
    }

    void append_i64(const std::int64_t value) noexcept
    {
        append_u64(
            static_cast<std::uint64_t>(value));
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return offset_;
    }

private:
    EncodedCommandEvidence& output_;
    std::size_t offset_{0};
};

} // namespace

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

    writer.append_u16(command_evidence_schema_version);
    writer.append_u16(0);

    writer.append_u64(record.ordinal);

    writer.append_u64(
        record.previous_record_fingerprint.value());

    writer.append_u64(
        record.entry.world_fingerprint_before.value());

    writer.append_u64(record.entry.observed_tick);

    writer.append_u16(
        record.entry.envelope.schema_version);

    writer.append_u16(
        static_cast<std::uint16_t>(
            record.entry.envelope.type));

    writer.append_u64(
        record.entry.envelope.client_id.value());

    writer.append_u64(
        record.entry.envelope.session_id.value());

    writer.append_u64(
        record.entry.envelope.sequence);

    writer.append_u64(
        record.entry.envelope.target_tick);

    writer.append_u64(
        record.entry.envelope.payload.entity_id.value());

    writer.append_i64(
        record.entry.envelope.payload.velocity.x.count());

    writer.append_i64(
        record.entry.envelope.payload.velocity.y.count());

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

simulation::StateFingerprint fingerprint_command_evidence(
    const CommandEvidenceRecord& record) noexcept
{
    const auto encoded =
        encode_command_evidence(record);

    return simulation::fingerprint_bytes(
        std::span<const std::byte>{
            encoded.data(),
            encoded.size(),
        });
}

bool verify_command_evidence_chain(
    const std::span<const CommandEvidenceRecord> records,
    const simulation::StateFingerprint expected_head) noexcept
{
    auto previous =
        simulation::StateFingerprint{0};

    for (std::size_t index = 0;
         index < records.size();
         ++index) {
        if (!std::in_range<std::uint64_t>(index)) {
            return false;
        }

        const auto& record = records[index];

        if (record.ordinal !=
            static_cast<std::uint64_t>(index)) {
            return false;
        }

        if (record.previous_record_fingerprint != previous) {
            return false;
        }

        previous =
            fingerprint_command_evidence(record);
    }

    return previous == expected_head;
}

std::span<const CommandEvidenceRecord>
CommandEvidenceLog::records() const noexcept
{
    return std::span<const CommandEvidenceRecord>{
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

simulation::StateFingerprint
CommandEvidenceLog::head_fingerprint() const noexcept
{
    return head_fingerprint_;
}

bool CommandEvidenceLog::verify() const noexcept
{
    return verify_command_evidence_chain(
        records(),
        head_fingerprint_);
}

void CommandEvidenceLog::prepare_append()
{
    if (!std::in_range<std::uint64_t>(
            records_.size())) {
        throw std::overflow_error{
            "command evidence ordinal cannot be represented"};
    }

    if (records_.size() == records_.max_size()) {
        throw std::length_error{
            "command evidence log reached its maximum size"};
    }

    records_.reserve(records_.size() + 1);
}

void CommandEvidenceLog::append(
    CommandEvidenceEntry entry) noexcept
{
    static_assert(
        std::is_nothrow_move_constructible_v<
            CommandEvidenceRecord>);

    assert(records_.size() < records_.capacity());
    assert(std::in_range<std::uint64_t>(records_.size()));

    CommandEvidenceRecord record{
        .ordinal =
            static_cast<std::uint64_t>(
                records_.size()),
        .previous_record_fingerprint =
            head_fingerprint_,
        .entry = std::move(entry),
    };

    const auto next_head =
        fingerprint_command_evidence(record);

    records_.push_back(std::move(record));
    head_fingerprint_ = next_head;
}

}
